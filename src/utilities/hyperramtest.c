#include <stdio.h>
#include <string.h>

#include <hal.h>
#include <memory.h>
#include <dirent.h>
#include <fileio.h>

unsigned long addr,time,speed;
unsigned char r2;
unsigned char cr0hi;
unsigned char cr0lo;
unsigned char id0hi;
unsigned char id0lo;
unsigned int mbs;

void bust_cache(void) {
  lpeek(0x8000100);
  lpeek(0x8000110);
  lpeek(0x8000120);
  lpeek(0x8000130);
  lpeek(0x8000140);
  lpeek(0x8000150);
  lpeek(0x8000160);
  lpeek(0x8000170);
  lpoke(0x8000100,0x99);
  lpoke(0x8000110,0x99);
  lpoke(0x8000120,0x99);
  lpoke(0x8000130,0x99);
  lpoke(0x8000140,0x99);
  lpoke(0x8000150,0x99);
  lpoke(0x8000160,0x99);
  lpoke(0x8000170,0x99);

  lpoke(0xbfffff2,0x02);
  lpoke(0xbfffff2,0x82);
}


void setup_hyperram(void)
{
  // Pre-fill all hyperram for convenience
  printf("Erasing hyperram");
  lpoke(0xbfffff2,0x82);  // cache on for much faster linear writes
  for(addr=0x8000000;(addr<0x9000000);addr+=0x8000)
    { lfill(addr,0x00,0x8000); printf("."); }
  printf("\n");

  
  /*
    Test complete HyperRAM, including working out the size.
  */
  printf("Determining size of Extra RAM");
  
  lpoke(0x8000000,0xbd);
  if (lpeek(0x8000000)!=0xbd) {
    printf("ERROR: $8000000 didn't hold its value.\n"
	   "Should be $BD, but saw $%02x\n",lpeek(0x8000000));
  }
  for(addr=0x8001000;(lpeek(0x8000000)==0xbd)&&(addr!=0x9000000);addr+=0x1000)
    {
      printf(".");
      lpoke(addr,0x55);

      bust_cache();
      
      if (lpeek(addr)!=0x55) {
	printf("$%08lx != $55 (saw $%02x)",addr,lpeek(addr));
	break;
      }
      lpoke(addr,0xAA);

      bust_cache();

      if (lpeek(addr)!=0xAA) {
	printf("$%08lx != $AA (saw $%02x)",addr,lpeek(addr));
	break;
      }

      bust_cache();
      
      if (lpeek(0x8000000)!=0xbd) {
	printf("$8000000 corrupted != $BD (saw $%02x)",addr,lpeek(0x8000000));
	break;
      }
    }

  lpoke(0xbfffff2,0x82);
  
  printf("\nPress any key to continue...\n");
  while(PEEK(0xD610)) POKE(0xD610,0);
  while(!PEEK(0xD610)) continue;
  while(PEEK(0xD610)) POKE(0xD610,0);
  
}

void main(void)
{
  POKE(0,65);
  POKE(0xD02F,0x47);
  POKE(0xD02F,0x53);

  setup_hyperram();

  printf("%c",0x93);

  // Turn cache back on before reading config registers etc
  lpoke(0xbfffff2,0x82);
  
  bust_cache();
  cr0hi=lpeek(0x9001000);
  bust_cache();
  cr0lo=lpeek(0x9001001);

  bust_cache();
  id0hi=lpeek(0x9000000);
  bust_cache();
  id0lo=lpeek(0x9000001);

  while(1) {

    while (PEEK(0xD610)) {
      setup_hyperram();
      POKE(0xD610,0);
      printf("%c",0x93);
    }
    
    printf("%cUpper limit of Extra RAM is $%08lx\n",0x13,addr);
    mbs=(unsigned int)((addr-0x8000000L)>>20L);
    printf("Extra RAM is %d MB\n",mbs);
    printf("Chip ID: %d rows, %d columns\n",
	   (id0hi&0x1f)+1,(id0lo>>4)+1);
    printf("Expected capacity: %d MB\n",
	   1<<((id0hi&0x1f)+1+(id0lo>>4)+1+1-20));
    printf("Vendor: ");
    switch(id0lo&0xf) {
    case 1: printf("Cypress"); break;
    case 3: printf("ISSI"); break;
    default: printf("<unknown>");
    }
    printf("\n");

    printf("Config: Powered up=%c,\n drive strength=$%x,\n",(cr0hi&0x80)?'Y':'N',(cr0hi>>4)&7);
    switch((cr0lo&0xf0)>>4) {
    case 0: printf(" 5 clock latency,\n"); break;
    case 1: printf(" 6 clock latency,\n"); break;
    case 14: printf(" 3 clock latency,\n"); break;
    case 15: printf(" 4 clock latency,\n"); break;
    default:
      printf("unknown latency clocks ($%x),\n",(cr0lo&0xf0)>>4);      
    }
    if (cr0lo&8) printf(" fixed latency,"); else printf(" variable latency,");
    printf("\n");
    if (cr0lo&28) printf(" legacy burst,"); else printf(" hybrid burst,");
    printf("\n");
    switch(cr0lo&3) {
    case 0: printf(" 128 byte burst length.\n"); break;
    case 1: printf(" 64 byte burst length.\n"); break;
    case 2: printf(" 16 byte burst length.\n"); break;
    case 3: printf(" 32 byte burst length.\n"); break;
    }

    // Test read speed of normal and extra ram  
    while(PEEK(0xD012)!=0x10)
      while(PEEK(0xD011)&0x80) continue;
    lcopy(0x20000L,0x40000L,32768);
    r2=PEEK(0xD012);
    printf("Copy Fast RAM to Fast RAM: ");
    // 63usec / raster
    time=(r2-0x10)*63;
    speed=32768000L/time;
    printf("%ld KB/sec\n",speed);
    
    // Hyperram Cache on
    lpoke(0xbfffff2,0x82);
    printf("With Cache enabled:\n");
    
    while(PEEK(0xD012)!=0x10)
      while(PEEK(0xD011)&0x80) continue;
    lcopy(0x8000000,0x40000,4096);
    r2=PEEK(0xD012);
    printf("Copy Extra RAM to Fast RAM: ");
    time=(r2-0x10)*63;
    speed=4096000L/time;
    printf("%ld KB/sec\n",speed);
    
    while(PEEK(0xD012)!=0x10)
      while(PEEK(0xD011)&0x80) continue;
    lcopy(0x40000,0x8000000,4096);
    r2=PEEK(0xD012);
    printf("Copy Fast RAM to Extra RAM: ");
    time=(r2-0x10)*63;
    speed=4096000L/time;
    printf("%ld KB/sec\n",speed);
    
    while(PEEK(0xD012)!=0x10)
      while(PEEK(0xD011)&0x80) continue;
    lcopy(0x8000000,0x8010000,4096);
    r2=PEEK(0xD012);
    printf("Copy Extra RAM to Extra RAM: ");
    time=(r2-0x10)*63;
    speed=4096000L/time;
    printf("%ld KB/sec\n",speed);

    while(PEEK(0xD012)!=0x10)
      while(PEEK(0xD011)&0x80) continue;
    lfill(0x8000000,0,4096);
    r2=PEEK(0xD012);
    printf("Fill Extra RAM: ");
    time=(r2-0x10)*63;
    speed=4096000L/time;
    printf("%ld KB/sec\n",speed);
    
    // Hyperram Cache off
    lpoke(0xbfffff2,0x02);
    printf("With Cache disabled:\n");
    
    while(PEEK(0xD012)!=0x10)
      while(PEEK(0xD011)&0x80) continue;
    lcopy(0x8000000,0x40000,4096);
    r2=PEEK(0xD012);
    printf("Copy Extra RAM to Fast RAM: ");
    time=(r2-0x10)*63;
    speed=4096000L/time;
    printf("%ld KB/sec\n",speed);
    
    while(PEEK(0xD012)!=0x10)
      while(PEEK(0xD011)&0x80) continue;
    lcopy(0x40000,0x8000000,4096);
    r2=PEEK(0xD012);
    printf("Copy Fast RAM to Extra RAM: ");
    time=(r2-0x10)*63;
    speed=4096000L/time;
    printf("%ld KB/sec\n",speed);
    
    while(PEEK(0xD012)!=0x10)
      while(PEEK(0xD011)&0x80) continue;
    lcopy(0x8000000,0x8010000,4096);
    r2=PEEK(0xD012);
    printf("Copy Extra RAM to Extra RAM: ");
    time=(r2-0x10)*63;
    speed=4096000L/time;
    printf("%ld KB/sec\n",speed);

    while(PEEK(0xD012)!=0x10)
      while(PEEK(0xD011)&0x80) continue;
    lfill(0x8000000,0,4096);
    r2=PEEK(0xD012);
    printf("Fill Extra RAM: ");
    time=(r2-0x10)*63;
    speed=4096000L/time;
    printf("%ld KB/sec\n",speed);
  }
  
  
}