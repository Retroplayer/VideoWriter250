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
static void filename(char *out,const unsigned char *name){int i,j=0;for(i=0;i<20;i++){unsigned char c=name[i];if(c==0)break;if(c==' '){out[j++]='_';continue;}out[j++]=(isalnum(c)||c=='-'||c=='_'||c=='$')?c:'_';}while(j&&out[j-1]=='_')j--;out[j]=0;if(!j)strcpy(out,"unnamed");}
static void write_text(FILE *f,const unsigned char *data,int len,int annotated,int rtf){int i=0,first=1;if(rtf)fputs("{\\rtf1\\ansi\\deff0\\paperw12240\\paperh15840\\margl1008\\margr1008\\margt1008\\margb1008{\\fonttbl{\\f0\\froman Times New Roman;}}\\f0\\fs22\\ql\n",f);
while(i<len){unsigned char c=data[i];if(i+6<=len&&memcmp(data+i,"\x99\x00\x00\x02\x04\x9b",6)==0){if(!first){fputs(rtf?"\\par\n":"\n",f);}i+=6;first=0;continue;}if(i+5<=len&&memcmp(data+i,"\x00\x00\x02\x04\x9b",5)==0){i+=5;continue;}if(i+3<=len&&memcmp(data+i,"\x02\x04\x9b",3)==0){i+=3;continue;}i++;if(c==0x99){fputs(rtf?"\\par\n":"\n",f);first=0;continue;}if(c>=32&&c<=126){if(rtf&&(c=='\\'||c=='{'||c=='}'))fputc('\\',f);fputc(c,f);first=0;continue;}if(c&&annotated)fprintf(f,"<%02X>",c);else if(c&&rtf)fprintf(f,"{\\*\\vwctrl %02X}",c);}
if(rtf)fputs("}\n",f);else fputc('\n',f);}
static int save(const char *dir,const char *stem,const char *ext,const unsigned char *p,int len,int mode){char path[1024];FILE *f;snprintf(path,sizeof(path),"%s/%s.%s",dir,stem,ext);f=fopen(path,"wb");if(!f){perror(path);return 0;}if(mode==0){if(fwrite(p,1,len,f)!=(size_t)len){fclose(f);return 0;}}else write_text(f,p,len,mode==2,mode==3);if(fclose(f)!=0)return 0;return 1;}
/* Capture combined stdout/stderr, but do not infer sector correctness from logs. */
static int gw_read_capture(const char *gw, const char *path) {
    char command[4096], line[2048];
    int status;
    FILE *pipe;
    const char *extra=getenv("VW_GW_EXTRA");
    if (!extra) extra="";
    if (snprintf(command,sizeof command,"\"%s\" read --format=ibm.scan --tracks=c=0-79:h=0 --retries=5 %s \"%s\" 2>&1",gw,extra,path)>=(int)sizeof command) return -1;
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
int main(int argc,char **argv){FILE *f;char tempimg[1024]={0};const char *input;int fromdrive=0;const char *cmd,*out="vw_output";int g,slot,allocated=0,success=0,skipped=0; int seen[1440]={0};
if(argc<2||argc>4){fprintf(stderr,"Usage: vwrecover [drive|IMAGE.img] [list|extract] [OUTPUT_DIR]\n");return 2;}
input=argv[1];
if(strcmp(input,"drive")==0){
    const char *gw=getenv("VW_GW");
    if(!gw||!*gw) gw="gw.exe";
    if(!temp_image(tempimg,sizeof tempimg)){fprintf(stderr,"Cannot allocate temporary image path\n");return 1;}
    puts("Reading VideoWriter disk with Greaseweazle...");fflush(stdout);
    if(gw_read_capture(gw,tempimg)!=0){fprintf(stderr,"Greaseweazle could not complete the disk read.\n");remove(tempimg);return 1;}
    input=tempimg;fromdrive=1;
}
cmd=argc>=3?argv[2]:"extract";if(strcmp(cmd,"list")&&strcmp(cmd,"extract")){fprintf(stderr,"Unknown command\n");return 2;}if(argc==4)out=argv[3];f=fopen(input,"rb");if(!f){perror(argv[1]);return 1;}if(fread(disk,1,SZ,f)!=SZ||fgetc(f)!=EOF){fprintf(stderr,"Invalid image size; expected 368640 bytes\n");fclose(f);return 1;}fclose(f);if(fromdrive)remove(tempimg);if(!looks_like_videowriter()){fprintf(stderr,"Image does not contain a recognizable VideoWriter allocation map and directory\n");return 1;}for(int i=0;i<1440;i++)allocated+=used(i);printf("VideoWriter 250 | 80 tracks, 18 sectors/track, 256 bytes/sector | allocated %d/1440\n",allocated);
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
if(lba<0||!used(lba)){printf("  skipped: header sector not allocated\n");skipped++;continue;}if(seen[lba]){printf("  alias: same document already recovered; skipping duplicate\n");continue;}seen[lba]=1;const unsigned char *h=sect(lba);int n=h[0]|(h[1]<<8),count=(n+255)/256;if(n<=0||count>127){printf("  skipped: invalid document byte count %d\n",n);skipped++;continue;}unsigned char *raw=malloc(count*256);if(!raw)return 1;int ok=1;for(int j=0;j<count;j++){int dt=h[2+2*j],ds=h[3+2*j],a=addr(dt,ds);if(a<0||!used(a)||a==lba){printf("  invalid data sector %d:%d (index %d)\n",dt,ds,j);ok=0;break;}memcpy(raw+j*256,sect(a),256);}if(ok){snprintf(stem,sizeof(stem),"%02d_%s",slot,name);ok=save(out,stem,"bin",raw,n,0)&&save(out,stem,"txt",raw,n,1)&&save(out,stem,"annotated.txt",raw,n,2)&&save(out,stem,"rtf",raw,n,3);if(ok){success++;printf("  extracted %d bytes across %d sectors\n",n,count);}}if(!ok)skipped++;free(raw);}
if(!strcmp(cmd,"extract")){printf("Extracted %d entries; skipped %d unresolved/stale entries. Read-only; source unchanged.\n",success,skipped);if(success==0){fprintf(stderr,"No recoverable documents found.\n");return 1;}}return 0;}
