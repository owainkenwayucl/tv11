#include "11.h"
#include "kd11a.h"
#include "kw11.h"
#include "kl11.h"
#include "args.h"

// in words
#define MEMSIZE (12*1024)

uint16 memory[MEMSIZE];

void
busadddev(Bus *bus, Busdev *dev)
{
	Busdev **bp;
	for(bp = &bus->devs; *bp; bp = &(*bp)->next)
		;
	*bp = dev;
	dev->next = nil;
}

int
dati_bus(Bus *bus)
{
	Busdev *bd;
	for(bd = bus->devs; bd; bd = bd->next)
		if(bd->dati(bus, bd->dev) == 0)
			return 0;
	return 1;
}

int
datip_bus(Bus *bus)
{
	return dati_bus(bus);
}

int
dato_bus(Bus *bus)
{
	Busdev *bd;
	for(bd = bus->devs; bd; bd = bd->next)
		if(bd->dato(bus, bd->dev) == 0)
			return 0;
	return 1;
}

int
datob_bus(Bus *bus)
{
	Busdev *bd;
	for(bd = bus->devs; bd; bd = bd->next)
		if(bd->datob(bus, bd->dev) == 0)
			return 0;
	return 1;
}

int dati_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 1; }
int dato_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 1; }
int datob_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 1; }
void reset_null(void *dev) { (void)dev; }
int svc_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 0; }

word
sgn(word w)
{
	return (w>>15)&1;
}

word
sxt(byte b)
{
	return (word)(int8_t)b;
}

void
loadmem(char *filename)
{
	FILE *f;
	char line[100], *s;
	word w;

	w = 0;
	f = fopen(filename, "r");
	if(f == nil)
		return;
	while(s = fgets(line, 100, f), s != nil){
		while(isspace(*s)) s++;
		if(*s == ':')
			w = strtol(s+1, nil, 8)>>1;
		if(*s >= '0' && *s <= '7')
			memory[w++] = strtol(s, nil, 8);
	}
	fclose(f);
}

int
loadpt(char *filename)
{
	FILE *f;
	byte hi, lo, s;
	word n, a, w;

	f = fopen(filename, "rb");
	if(f == nil){
		printf("can't open %s\n", filename);
		return 1;
	}

	for(;;){
		s = 0;
		if(fread(&lo, 1, 1, f) < 1) break;
		if(lo != 1)
			continue;
		s += lo;
		if(fread(&hi, 1, 1, f) < 1) break;
		s += hi;
		w = WD(hi, lo);
		if(w != 1)
			continue;
		if(fread(&lo, 1, 1, f) < 1) goto botch;
		s += lo;
		if(fread(&hi, 1, 1, f) < 1) goto botch;
		s += hi;
		n = WD(hi, lo);
		if(fread(&lo, 1, 1, f) < 1) goto botch;
		s += lo;
		if(fread(&hi, 1, 1, f) < 1) goto botch;
		s += hi;
		a = WD(hi, lo);
		if(a == 1)
			break;
		if(n == 0)
			continue;
		n -= 6;
		while(n--){
			if(fread(&lo, 1, 1, f) < 1) goto botch;
			s += lo;
			if(a & 1)
				memory[a>>1] = memory[a>>1]&0377 | lo<<8;
			else
				memory[a>>1] = memory[a>>1]&0177400 | lo;
			a++;
		}
		if(fread(&lo, 1, 1, f) < 1) goto botch;
		s += lo;
		if(s)
			goto botch;
	}
	fclose(f);
	return 0;
botch:
	printf("paper tape botch\n");
	fclose(f);
	return 1;
}

void
dumpmem(word start, word end)
{
	start >>= 1;
	end >>= 1;
	for(; start != end; start++)
		printf("%06o: %06o\n", start<<1, memory[start]);
}

KD11A cpu;
Bus bus;
KW11 kw11;
KL11 kl11;
Memory memdev = { memory, 0, MEMSIZE };
Busdev membusdev = { nil, &memdev, dati_mem, dato_mem, datob_mem, svc_null, nil, reset_null };
KE11 ke11;
Busdev kebusdev = { nil, &ke11, dati_ke11, dato_ke11, datob_ke11, svc_null, nil, reset_ke11 };
Busdev klbusdev = { nil, &kl11, dati_kl11, dato_kl11, datob_kl11, svc_kl11, bg_kl11, reset_kl11 };
Busdev kwbusdev = { nil, &kw11, dati_kw11, dato_kw11, datob_kw11, svc_kw11, bg_kw11, reset_kw11 };

char *argv0;

void
usage(void)
{
	fprintf(stderr, "usage: %s\n", argv0);
	exit(0);
}

#ifdef AUTODIAG
int diagpassed;

int
rundiag(KD11A *cpu, char *ptfile)
{
	word sw;
	memset(memory, 0, MEMSIZE*2);
	if(loadpt(ptfile)){
		fprintf(stderr, "couldn't open file '%s'\n", ptfile);
		return 1;
	}
	sw = cpu->sw;
	reset(cpu);
	memset(cpu, 0, sizeof(KD11A));
	cpu->bus = &bus;
	cpu->sw = sw;
	diagpassed = 0;
	cpu->r[7] = 0200;	
	run(cpu);
	if(diagpassed){
		printf("passed %s\n", ptfile);
		return 0;
	}
	return 1;
}
#endif


int
main(int argc, char *argv[])
{
	memset(&cpu, 0, sizeof(KD11A));
	memset(&bus, 0, sizeof(Bus));
	cpu.bus = &bus;
	busadddev(&bus, &membusdev);
	busadddev(&bus, &kwbusdev);
	busadddev(&bus, &klbusdev);
	busadddev(&bus, &kebusdev);

	ARGBEGIN{
	}ARGEND;

	if(argc < 0)
		usage();

	loadmem("mem.txt");

	reset(&cpu);

	/* open a tty if it exists */
	kl11.ttyfd = open("/tmp/tty", O_RDWR);
	printf("tty connected to %d\n", kl11.ttyfd);

#ifdef AUTODIAG
	printf("running diags\n");
	/* basic tests */
	if(rundiag(&cpu, "maindec/ZKAAA0.BIN")) return 0;	// branch
	if(rundiag(&cpu, "maindec/ZKABA0.BIN")) return 0;	// con branch
	if(rundiag(&cpu, "maindec/ZKACA0.BIN")) return 0;	// unary
	if(rundiag(&cpu, "maindec/ZKADA0.BIN")) return 0;	// binary
	if(rundiag(&cpu, "maindec/ZKAEA0.BIN")) return 0;	// rot/shf
	if(rundiag(&cpu, "maindec/ZKAFA0.BIN")) return 0;	// cmp
	if(rundiag(&cpu, "maindec/ZKAGA0.BIN")) return 0;	// cmp not
	if(rundiag(&cpu, "maindec/ZKAHA0.BIN")) return 0;	// move
	if(rundiag(&cpu, "maindec/ZKAIA0.BIN")) return 0;	// bis bic bit
	if(rundiag(&cpu, "maindec/ZKAJA0.BIN")) return 0;	// add
	if(rundiag(&cpu, "maindec/ZKAKA0.BIN")) return 0;	// sub
	if(rundiag(&cpu, "maindec/ZKALA0.BIN")) return 0;	// jmp
	if(rundiag(&cpu, "maindec/ZKAMA0.BIN")) return 0;	// rts rti jsr

	/* 11/40 traps */
	if(rundiag(&cpu, "maindec/BKDMD0.BIC")) return 0;

	/* 11/40 instructions */
	if(rundiag(&cpu, "maindec/CKBAB0.BIC")) return 0;	// sxt
	if(rundiag(&cpu, "maindec/CKBBB0.BIC")) return 0;	// sob
	if(rundiag(&cpu, "maindec/CKBCC0.BIC")) return 0;	// xor
	if(rundiag(&cpu, "maindec/CKBDC0.BIC")) return 0;	// mark
	if(rundiag(&cpu, "maindec/CKBEC0.BIC")) return 0;	// rtt
	if(rundiag(&cpu, "maindec/CKBFD0.BIC")) return 0;	// stack limit

	/* EIS */
	if(rundiag(&cpu, "maindec/CKBIB0.BIC")) return 0;	// ash
	if(rundiag(&cpu, "maindec/CKBJA0.BIC")) return 0;	// ashc
	if(rundiag(&cpu, "maindec/CKBKA0.BIC")) return 0;	// mul
	if(rundiag(&cpu, "maindec/CKBLA0.BIC")) return 0;	// div

	/* KT11 */
	if(rundiag(&cpu, "maindec/BKTAD1.BIC")) return 0;	// basic logic
	if(rundiag(&cpu, "maindec/BKTBB0.BIC")) return 0;	// access keys
	if(rundiag(&cpu, "maindec/BKTCB0.BIC")) return 0;	// moves
	if(rundiag(&cpu, "maindec/BKTDC0.BIC")) return 0;	// states
	if(rundiag(&cpu, "maindec/BKTFD0.BIC")) return 0;	// abort

	/* system exerciser with KT11 */
/*	// no idea how this works
	cpu.sw = 0100001;
	cpu.sw |= 0370;	// disable a couple of devices
	if(rundiag(&cpu, "maindec/BKTGD1.BIC")) return 0;	// exerciser
*/
	printf("passed all diagnostics\n");
		return 0;
#endif

	cpu.r[7] = 0200;
//	cpu.sw = 0104000;
	run(&cpu);

	return 0;
}
