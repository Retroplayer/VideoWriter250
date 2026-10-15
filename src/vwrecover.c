/* VideoWriter 250 native read-only console extractor, C99. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#include <sys/wait.h>
#define MKDIR(p) mkdir(p,0755)
#endif
#define SZ 368640
#define SS 256
#define SPT 18
static unsigned char disk[SZ];
static int used(int lba){return (disk[lba/8]>>(lba%8))&1;}
static int addr(int t,int s){return t>=0&&t<80&&s>=0&&s<18?t*18+s:-1;}
static unsigned char *sect(int lba){return disk+lba*SS;}
/* There are genuine VideoWriter document disks with no 'WV' identifier.
 * Recognize the disk by its directory and allocation structures instead.
 * Do not assume a readable directory entry proves the document is recoverable.
 */
static int looks_like_videowriter(void) {
    int i, plausible = 0;
    if (!used(0) || !used(1)) return 0;
    for (i=0; i<4; i++) {
        const unsigned char *r=sect(1)+64*i;
        int j, has_name=0, a;
        if (r[0]==0) continue;
        for (j=6;j<26;j++) {
            if (r[j] && (r[j]<32 || r[j]>126)) return 0;
            if (r[j]!=' ' && r[j]!=0) has_name=1;
        }
        a=addr(r[26],r[27]);
        if (!has_name || a<0 || !used(a)) continue;
        plausible++;
    }
    return plausible>0;
}

/* ImageDisk (.IMD) sector loader. ImageDisk records may be out of track/sector
 * order, use compressed filler bytes, and flag missing or CRC-bad sectors.
 * No failed sector is ever represented to the filesystem as a valid zero sector.
 */
static unsigned char sector_status[1440]; /* 0 unreadable/absent, 1 reliable */
static int image_is_imd=0;
static int bad_sectors=0;
static int sector_good(int lba) {
    return lba>=0 && lba<1440 && sector_status[lba];
}
static int load_raw_image(FILE *f) {
    if(fread(disk,1,SZ,f)!=SZ || fgetc(f)!=EOF) {
        fprintf(stderr,"Invalid raw image size; expected %d bytes\n",SZ);
        return 0;
    }
    memset(sector_status,1,sizeof sector_status);
    return 1;
}
static int load_imd_image(FILE *f) {
    int c,found_eof=0,tracks=0,good=0,invalid=0,duplicates=0;
    unsigned char th[5],ids[255],cmap[255],hmap[255];
    if(fseek(f,0,SEEK_SET)!=0)return 0;
    if(fread(th,1,4,f)!=4 || memcmp(th,"IMD ",4)!=0) {
        fprintf(stderr,"Invalid ImageDisk header\n");return 0;
    }
    /* A finite comment limit protects against malformed inputs. */
    for(size_t n=4;n<65536;n++) {
        c=fgetc(f);
        if(c==0x1a){found_eof=1;break;}
        if(c==EOF){break;}
    }
    if(!found_eof){fprintf(stderr,"Truncated/invalid ImageDisk comment header\n");return 0;}
    memset(disk,0,sizeof disk);
    memset(sector_status,0,sizeof sector_status);
    image_is_imd=1;
    for(;;) {
        int mode=fgetc(f),cyl,headfield,count,sizecode,head;
        if(mode==EOF)break;
        cyl=fgetc(f);headfield=fgetc(f);count=fgetc(f);sizecode=fgetc(f);
        if(cyl==EOF||headfield==EOF||count==EOF||sizecode==EOF||
           mode>5||count==0||sizecode>6) {
            fprintf(stderr,"Invalid ImageDisk track header (track record %d)\n",tracks);
            return 0;
        }
        head=headfield&0x3f;
        if(fread(ids,1,count,f)!=(size_t)count)goto truncated;
        if(headfield&0x80) {
            if(fread(cmap,1,count,f)!=(size_t)count)goto truncated;
        } else memset(cmap,cyl,(size_t)count);
        if(headfield&0x40) {
            if(fread(hmap,1,count,f)!=(size_t)count)goto truncated;
        } else memset(hmap,head,(size_t)count);
        int sectorbytes=128<<sizecode;
        for(int i=0;i<count;i++) {
            int typ=fgetc(f),compressed,broken,valid, lba;
            unsigned char data[8192],fill=0;
            if(typ==EOF||typ>8)goto truncated;
            compressed=(typ!=0 && (typ%2)==0);
            broken=(typ>=5);
            if(typ!=0) {
                if(compressed) {
                    c=fgetc(f);if(c==EOF)goto truncated;
                    fill=(unsigned char)c;
                    memset(data,fill,(size_t)sectorbytes);
                } else if(fread(data,1,(size_t)sectorbytes,f)!=(size_t)sectorbytes)goto truncated;
            }
            valid=(mode==5 && sectorbytes==256 && cyl<80 && head==0 &&
                   cmap[i]==cyl && hmap[i]==head && ids[i]<18);
            if(!valid) {
                /* Keep parsing other encodings/sides, but never map an
                 * incompatible physical record into a VideoWriter sector. */
                if(head!=0 || cyl>=80 || mode!=5 || sectorbytes!=256 ||
                   cmap[i]!=cyl || hmap[i]!=head || ids[i]>=18)invalid++;
                continue;
            }
            lba=addr(cyl,ids[i]);
            if(sector_status[lba]) {
                duplicates++;
                if(typ==0 || broken)continue;
                if(memcmp(sect(lba),data,256)!=0) {
                    fprintf(stderr,"Conflicting duplicate sector %d:%d: refusing image\n",cyl,ids[i]);
                    return 0;
                }
                continue;
            }
            if(typ==0 || broken)continue;
            memcpy(sect(lba),data,256);
            sector_status[lba]=1;
            good++;
        }
        tracks++;
    }
    bad_sectors=1440-good;
    printf("ImageDisk: %d tracks, %d reliable sectors, %d missing/CRC-bad, %d ignored incompatible records, %d duplicates\n",
           tracks,good,bad_sectors,invalid,duplicates);
    if(!good){fprintf(stderr,"No VideoWriter 250 sectors found in IMD image\n");return 0;}
    return 1;
truncated:
    fprintf(stderr,"Truncated or malformed ImageDisk sector record\n");
    return 0;
}
static int load_input_image(FILE *f) {
    unsigned char magic[4];
    if(fread(magic,1,4,f)!=4 || fseek(f,0,SEEK_SET)!=0)return 0;
    if(memcmp(magic,"IMD ",4)==0)return load_imd_image(f);
    return load_raw_image(f);
}

static void filename(char *out,const unsigned char *name){int i,j=0;for(i=0;i<20;i++){unsigned char c=name[i];if(c==0)break;if(c==' '){out[j++]='_';continue;}out[j++]=(isalnum(c)||c=='-'||c=='_'||c=='$')?c:'_';}while(j&&out[j-1]=='_')j--;out[j]=0;if(!j)strcpy(out,"unnamed");}
static void write_text(FILE *f,const unsigned char *data,int len,int annotated,int rtf){int i=0,first=1;if(rtf)fputs("{\\rtf1\\ansi\\deff0\\paperw12240\\paperh15840\\margl1008\\margr1008\\margt1008\\margb1008{\\fonttbl{\\f0\\froman Times New Roman;}}\\f0\\fs22\\ql\n",f);
while(i<len){unsigned char c=data[i];if(i+6<=len&&memcmp(data+i,"\x99\x00\x00\x02\x04\x9b",6)==0){if(!first){fputs(rtf?"\\par\n":"\n",f);}i+=6;first=0;continue;}if(i+5<=len&&memcmp(data+i,"\x00\x00\x02\x04\x9b",5)==0){i+=5;continue;}if(i+3<=len&&memcmp(data+i,"\x02\x04\x9b",3)==0){i+=3;continue;}i++;if(c==0x99){fputs(rtf?"\\par\n":"\n",f);first=0;continue;}if(c>=32&&c<=126){if(rtf&&(c=='\\'||c=='{'||c=='}'))fputc('\\',f);fputc(c,f);first=0;continue;}if(c&&annotated)fprintf(f,"<%02X>",c);else if(c&&rtf)fprintf(f,"{\\*\\vwctrl %02X}",c);}
if(rtf)fputs("}\n",f);else fputc('\n',f);}
/* Short-document data records observed in VW1: header 01 00,
 * 25 zero bytes, FF, text bytes, and 96 01 terminator.
 * Keep BIN/annotated raw; select only the recognized text span for TXT/RTF.
 * This is deliberately NOT a generic control-code interpreter.
 */
static int short_document_text(const unsigned char *raw,int len,int *offset,int *length) {
    int i;
    if(len<31 || raw[0]!=1 || raw[1]!=0 || raw[27]!=0xff) return 0;
    for(i=2;i<27;i++) if(raw[i]!=0) return 0;
    for(i=28;i+1<len;i++) {
        if(raw[i]==0x96 && raw[i+1]==0x01) {
            *offset=28;
            *length=i-28;
            return 1;
        }
    }
    return 0;
}
static int save(const char *dir,const char *stem,const char *ext,const unsigned char *p,int len,int mode){char path[1024];FILE *f;snprintf(path,sizeof(path),"%s/%s.%s",dir,stem,ext);f=fopen(path,"wb");if(!f){perror(path);return 0;}if(mode==0){if(fwrite(p,1,len,f)!=(size_t)len){fclose(f);return 0;}}else write_text(f,p,len,mode==2,mode==3);if(fclose(f)!=0)return 0;return 1;}
/* Capture combined stdout/stderr, but do not infer sector correctness from logs. */
static int gw_read_capture(const char *gw, const char *path, const char *drive) {
    char command[4096], line[2048];
    int status;
    FILE *pipe;
    const char *extra=getenv("VW_GW_EXTRA");
    if (!extra) extra="";
    if (snprintf(command,sizeof command,"\"%s\" read --format=ibm.scan --tracks=c=0-79:h=0 --retries=5 --drive=%s %s \"%s\" 2>&1",gw,drive,extra,path)>=(int)sizeof command) return -1;
#ifdef _WIN32
    pipe=_popen(command,"r");
#else
    pipe=popen(command,"r");
#endif
    if (!pipe) { perror("Starting Greaseweazle"); return -1; }
    while (fgets(line,sizeof line,pipe)) {
        fputs("[GW] ",stdout);
        fputs(line,stdout);
        fflush(stdout);
    }
#ifdef _WIN32
    status=_pclose(pipe);
#else
    status=pclose(pipe);
    if (status>=0 && WIFEXITED(status)) status=WEXITSTATUS(status);
#endif
    printf("[GW] process exit status: %d\n",status);
    return status;
}
static int temp_image(char *path, size_t size) {
#ifdef _WIN32
    char base[MAX_PATH];
    if (size<MAX_PATH) return 0;
    if (!GetTempPathA(MAX_PATH,base)) return 0;
    if (!GetTempFileNameA(base,"vwr",0,path)) return 0;
    DeleteFileA(path);
    return 1;
#else
    const char *tmpl="/tmp/vwrecover_XXXXXX.img";
    int fd;
    if (size<=strlen(tmpl)) return 0;
    strcpy(path,tmpl);
    fd=mkstemps(path,4);
    if (fd<0) return 0;
    close(fd);
    unlink(path);
    return 1;
#endif
}
int main(int argc,char **argv){FILE *f;char tempimg[1024]={0};const char *input;int fromdrive=0;const char *cmd="extract",*out="vw_output",*drive="0";int g,slot,allocated=0,success=0,skipped=0; int seen[1440]={0};
int arg=2;
if(argc<2||argc>5){fprintf(stderr,"Usage: vwrecover drive [0|1|2|A|B] [list|extract] [OUTPUT_DIR]\n       vwrecover IMAGE.img|IMAGE.imd [list|extract] [OUTPUT_DIR]\n");return 2;}
input=argv[1];
if(strcmp(input,"drive")==0){
    const char *gw=getenv("VW_GW");
    if(arg<argc && (strcmp(argv[arg],"0")==0||strcmp(argv[arg],"1")==0||strcmp(argv[arg],"2")==0||strcmp(argv[arg],"A")==0||strcmp(argv[arg],"B")==0||strcmp(argv[arg],"a")==0||strcmp(argv[arg],"b")==0)) drive=argv[arg++];
    else if(arg<argc && strcmp(argv[arg],"extract")!=0 && strcmp(argv[arg],"list")!=0){
        fprintf(stderr,"Invalid drive selector: %s (use 0, 1, 2, A or B)\n",argv[arg]);return 2;
    }
    if(!gw||!*gw) gw="gw.exe";
    if(!temp_image(tempimg,sizeof tempimg)){fprintf(stderr,"Cannot allocate temporary image path\n");return 1;}
    printf("Reading VideoWriter disk with Greaseweazle, drive %s...\n",drive);fflush(stdout);
    if(gw_read_capture(gw,tempimg,drive)!=0){fprintf(stderr,"Greaseweazle could not complete the disk read.\n");remove(tempimg);return 1;}
    input=tempimg;fromdrive=1;
}
if(arg<argc) cmd=argv[arg++];
if(strcmp(cmd,"list")&&strcmp(cmd,"extract")){fprintf(stderr,"Unknown command\n");return 2;}
if(arg<argc) out=argv[arg++];
if(arg<argc){fprintf(stderr,"Too many arguments\n");return 2;}
f=fopen(input,"rb");if(!f){perror(argv[1]);return 1;}if(!load_input_image(f)){fclose(f);return 1;}fclose(f);if(fromdrive)remove(tempimg);if(!sector_good(0)||!sector_good(1)){fprintf(stderr,"Allocation bitmap or user directory missing/damaged; cannot safely extract\n");return 1;}if(!looks_like_videowriter()){fprintf(stderr,"Image does not contain a recognizable VideoWriter allocation map and directory\n");return 1;}for(int i=0;i<1440;i++)allocated+=used(i);printf("VideoWriter 250 | 80 tracks, 18 sectors/track, 256 bytes/sector | allocated %d/1440\n",allocated);
if(!strcmp(cmd,"extract")){
    static char unique[1024];
    struct stat st;
    if(stat(out,&st)==0){
        int suffix;
        for(suffix=1;suffix<10000;suffix++){
            snprintf(unique,sizeof unique,"%s_%03d",out,suffix);
            if(stat(unique,&st)!=0)break;
        }
        if(suffix==10000){fprintf(stderr,"Too many existing recovery directories\n");return 1;}
        out=unique;
    }
    if(MKDIR(out)!=0){perror(out);return 1;}
    printf("Saving recovered documents in: %s\n",out);
}
for(g=0;g<2;g++)for(slot=0;slot<4;slot++){const unsigned char *r=sect(g?16:1)+64*slot;char name[64],stem[128];int t=r[26],s=r[27],lba=addr(t,s);if(!r[0])continue;filename(name,r+6);printf("%-6s slot=%d %-22s -> %02d:%02d  %s\n",g?"system":"user",slot,name,t,s,lba>=0?(used(lba)?"allocated":"FREE/STALE"):"INVALID");if(strcmp(cmd,"extract")||g)continue;
if(lba<0||!used(lba)||!sector_good(lba)){printf("  skipped: header sector free, unreadable or CRC-bad\n");skipped++;continue;}if(seen[lba]){printf("  alias: same document already recovered; skipping duplicate\n");continue;}seen[lba]=1;const unsigned char *h=sect(lba);int n=h[0]|(h[1]<<8),count=(n+255)/256;if(n<=0||count>127){printf("  skipped: invalid document byte count %d\n",n);skipped++;continue;}unsigned char *raw=malloc(count*256);if(!raw)return 1;int ok=1;for(int j=0;j<count;j++){int dt=h[2+2*j],ds=h[3+2*j],a=addr(dt,ds);if(a<0||!used(a)||a==lba||!sector_good(a)){printf("  missing, CRC-bad or invalid data sector %d:%d (index %d)\n",dt,ds,j);ok=0;break;}memcpy(raw+j*256,sect(a),256);}if(ok){snprintf(stem,sizeof(stem),"%02d_%s",slot,name);{
    int textoff=0,textlen=n;
    int shortdoc=short_document_text(raw,n,&textoff,&textlen);
    if(shortdoc) printf("  recognized short-document text record at +%d, %d bytes\n",textoff,textlen);
    ok=save(out,stem,"bin",raw,n,0)&&
       save(out,stem,"annotated.txt",raw,n,2)&&
       save(out,stem,"txt",raw+textoff,textlen,1)&&
       save(out,stem,"rtf",raw+textoff,textlen,3);
}if(ok){success++;printf("  extracted %d bytes across %d sectors\n",n,count);}}if(!ok)skipped++;free(raw);}
if(!strcmp(cmd,"extract")){printf("Extracted %d entries; skipped %d unresolved/stale entries. Read-only; source unchanged.\n",success,skipped);if(success==0){fprintf(stderr,"No recoverable documents found.\n");return 1;}}return 0;}
