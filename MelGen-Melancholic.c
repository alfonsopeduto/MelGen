/*

 MelGen 
 Version: Cminor, 4/4, Melancholic, Mid-tempo
 
 by Alfonso Peduto
 
 This program is part of the MelGen Algorithm Suite
 a collection of algorithms for music generative purposes.
 
 More info about this project can be perused at www.alfonsopedutolab.com
 (C) Alfonso Peduto, 2012-2018
 
 */

#include <stdio.h>
#include <time.h>

int new_score(void);
int add_note(int /*pitch*/, int /*volume*/, int /*instr*/,
             int /*when*/, int /*length*/);
int play_score(void);
int save_score(char *);

#define SUCCESS		(0)
#define ERR_MEMORY	(1)
#define ERR_FAIL_SAVE	(2)

typedef struct note {
    int          pitch;
    int          volume;
    int          instr;
    int          when;
    int          length;
    struct note* next;
} Note;

extern Note* score;

#define MThd     (0x4d546864)
struct Hdr_Chk {
  int     type;     
  int     length;  
  short   format;   
  short   ntrks;    
  short   division; 
};

#define MTrk     (0x4d54726b)
struct Trk_Hdr {
  int     type;     /* Must be 'MTrk', 0x4d54726b */
  int     length;   /* no. of bytes in the track chunk following */
};

#define NOTEON  (0x90)
struct NoteOn {
    char    eventch;
    char    pitch;
    char    velocity;
};
#define PROGCHN  (0xc0)


#include <stdlib.h>
#include <stdio.h>

Note *score = NULL;

/* Internal function to free a score structure */
static void destroy_score(Note* sc)
{
    while (sc!=NULL) {
      Note* nxt = sc->next;
      free(sc);
      sc = nxt;
    }
}

/* Public interface function to start a new score */
/* Should it check to see if score was played or saved? */
int new_score(void)
{
    if (score!=NULL) destroy_score(score);
    score = NULL;
    return SUCCESS;
}

/* Create a new note structure and populate */
static Note *create_note(int pitch, int volume, int instr,
                         int when, int length)
{
    Note *newnote = (Note *)malloc(sizeof(Note));
    if (newnote==NULL) {
      /* Malloc failed so return error */
      return NULL;
    }
    newnote->pitch = pitch;
    newnote->volume = volume;
    newnote->instr = instr;
    newnote->when = when;
    newnote->length = length;
    newnote->next = NULL;
    return newnote;
}

int add_note(int pitch, int volume, int instr,
             int when, int length)
{
    Note *newnote = create_note(pitch, volume, instr,
                               when, length);
    Note *note = score;
    Note *previous = NULL;
    if (newnote==NULL) return ERR_MEMORY;

    while (1) {
      if (note==NULL) {
        if (previous==NULL) {
          score = newnote;
        }
        else {
          newnote->next = note;
          previous->next = newnote;
        }
        break;
      }
      if (when<=note->when) {
        /* Note comes before current note */
        newnote->next = note;
        if (previous==NULL) {
          score = newnote;
        }
        else {
          newnote->next = note;
          previous->next = newnote;
        }
        break;
      }
      /* Comes later */
      previous = note;
      note = note->next;
    }
    if (volume!=0)
      return add_note(pitch, 0, instr, when+length, 0);
    else return SUCCESS;
}


/* Support code to write MIDI files */

static int trk_cnt = 0;

static void print_int_be(FILE* f, int n)
{
    putc((n>>24)&0xff,f);
    putc((n>>16)&0xff,f);
    putc((n>>8)&0xff,f);
    putc(n&0xff,f);
    trk_cnt += 4;
}

static void print_short_be(FILE* f, short n)
{
    putc((n>>8)&0xff,f);
    putc(n&0xff,f);
    trk_cnt += 2;
}

static void print_variable(FILE *f, int n)
{
    if (n==0) {                 /* Treat zero as special case */
      putc(0, f);
      trk_cnt += 1;
    }
    else {                      /* need up to 4 bytes */
      int i = 3;
      char buffer[4];
      buffer[0] = n&0x7f;
      buffer[1] = (n>>7)&0x7f;
      buffer[2] = (n>>14)&0x7f;
      buffer[3] = (n>>21)&0x7f;
      while (buffer[i]==0) i--; /* Find first non-zero 7bit byte */
      do {
        if (i!=0) putc(buffer[i]|0x80, f);
        else putc(buffer[i], f); /* Last byte does not have 0x80 */
        trk_cnt++;
        i--;
      } while (i>=0);
    }
}

static FILE* open_MIDI_file(char *name)
{
    FILE *fout = fopen(name, "wb");
    if (fout==NULL) return NULL;
    /* Write Header */
    print_int_be(fout, MThd);         /* Type */
    print_int_be(fout, 6);            /* length = 6 */
    print_short_be(fout, 0);          /* format single track */
    print_short_be(fout, 1);          /* ntrks = 1 */
    print_short_be(fout, 0x1e0);     /* division in milliseconds */
    /* Track Header */
    print_int_be(fout, MTrk);
    print_int_be(fout, 000);          /* length to be overwritten */
    trk_cnt = 0;
    return fout;
}

typedef struct {
  int instr;
  int count;
} CHAN;
static CHAN chans[16] = { {0,0},{0,0},{0,0},{0,0},
                          {0,0},{0,0},{0,0},{0,0},
                          {0,0},{0,0},{0,0},{0,0},
                          {0,0},{0,0},{0,0},{0,0}};

static int map_instr_chan(FILE *fout, int instr, int t)
{
    int i;
    int j=15;
    int low = 0x7fff;
    for (i=0; i<16; i++) {
      if (chans[i].instr==instr) {
        chans[i].count++;
        return i;
      }
      if (chans[i].count<low) {
        low = chans[i].count;
        j = i;
      }
    }
    /* Need program change */
    print_variable(fout, t);
    putc(PROGCHN|j, fout);
    putc(instr-1, fout);
    chans[j].instr = instr;
    chans[j].count = 1;
    return j;
}


static void write_score(FILE *fout)
{
    Note *note = score;
    int time = 0;
    while (note!=NULL) {
      /* Night need a program change */
      int chan = map_instr_chan(fout, note->instr,note->when - time);
      /* First the time delay */
      print_variable(fout, note->when - time);
      time = note->when;
      putc(NOTEON|chan, fout);
      putc(note->pitch, fout);
      putc(note->volume, fout);
      trk_cnt += 3;
      note = note->next;
    }
    /* Need an end-of-track meta event after time zero */
    putc(0, fout); putc(0xff, fout); putc(0x2f, fout); putc(0x00, fout);
    trk_cnt += 4;
}

static int close_MIDI_file(FILE *fout)
{
    int err;
    err  = fseek(fout, 18L, SEEK_SET); /* rewind to length field */
    print_int_be(fout, trk_cnt);
    return fclose(fout);
}


int save_score(char *name)
{
    FILE *f = open_MIDI_file(name);
    if (f==NULL) return ERR_FAIL_SAVE;
    write_score(f);
    return close_MIDI_file(f);
}






/* CounterpointRULES */
int cprules(int i, int melody[])
{

int j=i;


/* start with do */
if(i==0)
{ if(melody[i] != 60) { j=i-1;} }


/* start with a jump! (second note must be greater than 2 semitones) */
if(j==i)
{
if(i==1)
{ if(abs(melody[i]-melody[i-1])>2) { j=i-1;} }
}


/*  do not allow two repeated notes  */
if(j==i)
{
if(i>1)
{ if(melody[i]==melody[i-1] && melody[i]==melody[i-2]) {j=i-2;} }
}

/*  do not allow notes apart in the sequence by 4 elements to be too close (within 5 semitones)  */
if(j==i)
{
if(i>4)
{ if(melody[i]==melody[i-3]<5) {j=i-4;} }
}

/*  do not allow repeated notes  */
if(j==i)
{
if(i>0)
{ if(melody[i]==melody[i-1]) {j=i-1;} }
}

/*  do not allow two leaps such that they are greater than 2 and 3 semitones  */
if(j==i)
{
if(i>1)
{ if(abs(melody[i]==melody[i-1])>2 && abs(melody[i-1]==melody[i-2])>3) {j=i-2;} }
}


/*  do not allow three consecutive leaps greater than 2 semitones  */
if(j==i)
{
if(i>3)
{ if(abs(melody[i]==melody[i-1])>2 && abs(melody[i]==melody[i-2])>2 && abs(melody[i-2]==melody[i-3])>2) {j=i-3;} }
}

if(j==i)
{
if(i>1)
{ if( abs(melody[i]-melody[i-1])>4 && abs(melody[i]-melody[i-1])>2 ) { j=i-1;}}
}

/*  do not allow the following: a positive leap greater than 3 semitones followed by another positive leap  */
if(j==i)
{
if(i>1)
{ if( (melody[i-1]-melody[i-2])>3 && (melody[i]-melody[i-1]>0) ) { j=i-1; }}
}

/* do not allow the following: a negative leap greater than 3 semitones followed by another negative leap */
if(j==i)
{
if(i>1)
{ if( (melody[i-1]-melody[i-2])<-3 && (melody[i]-melody[i-1]<0) ) { j=i-1;}}
}

/* do not allow sequences of 4 notes to be greater than 10 semitones */
if(j==i)
{
if(i>4)
{ if(abs(melody[i]-melody[i-4])>10) {j=i-1;}}
}


/*do not allow  "big leaps" (greater than 8 semitones) */
if(j==i)
{
if(i>1)
{ if(abs(melody[i]-melody[i-1])>8) { j=i-1;} }
}

/*do not allow too many repeated notes */
if(j==i)
{
if(i>3)
{ if(melody[i]==melody[i-1] && melody[i-2]==melody[i-3]) { j=i-1;}}
}

if(j==i)
{
if(i>3)
{ if(melody[i]==melody[i-3] && melody[i-2]==melody[i-1]) { j=i-1;}}
}


/* macro compositional considerations - do not allow too many climaxes*/
if(j==i)
{
if(i>20)
{ if(abs(melody[i]==melody[i-10])>10 && melody[i-10]==melody[i-20]>10) { j=i-19;}}
}

if(j==i)
{
if(i>20)
{ if(abs(melody[i]==melody[i-10])<5 && melody[i-10]==melody[i-20]<5) { j=i-19;}}
}

return j;


}



/* create midi files */
int createmusic(int FINALrythms[], int melody[], int TOTnotes)
{

int i=0;
int time=0;

new_score();
for(i=0; i<TOTnotes; i++)
{
add_note(melody[i], 64, 0, time, FINALrythms[i]);
time = time + FINALrythms[i];
}
save_score("1_Melody.mid");



new_score();
for(i=0; i<TOTnotes; i++)
{

if(melody[i]==60) { add_note(51, 64, 0, time, FINALrythms[i]); }
if(melody[i]==62) { add_note(53, 64, 0, time, FINALrythms[i]); }
if(melody[i]==63) { add_note(55, 64, 0, time, FINALrythms[i]); }
if(melody[i]==65) { add_note(62, 64, 0, time, FINALrythms[i]); }
if(melody[i]==67) { add_note(63, 64, 0, time, FINALrythms[i]); }
if(melody[i]==68) { add_note(60, 64, 0, time, FINALrythms[i]); }
if(melody[i]==70) { add_note(62, 64, 0, time, FINALrythms[i]); }
if(melody[i]==72) { add_note(63, 64, 0, time, FINALrythms[i]); }
if(melody[i]==74) { add_note(65, 64, 0, time, FINALrythms[i]); }
if(melody[i]==75) { add_note(67, 64, 0, time, FINALrythms[i]); }
if(melody[i]==77) { add_note(74, 64, 0, time, FINALrythms[i]); }
if(melody[i]==79) { add_note(75, 64, 0, time, FINALrythms[i]); }
if(melody[i]==80) { add_note(72, 64, 0, time, FINALrythms[i]); }
if(melody[i]==82) { add_note(74, 64, 0, time, FINALrythms[i]); }
if(melody[i]==84) { add_note(75, 64, 0, time, FINALrythms[i]); }

time = time + FINALrythms[i];


}
save_score("2_Melody-Counterpoint.mid");


new_score();
for(i=0; i<TOTnotes; i++)
{
time = time + FINALrythms[i];

if(time==0 | time%1920==0) 
{ 
if(melody[i]==60) { add_note(43, 64, 0, time, 1920); }
if(melody[i]==63) { add_note(48, 64, 0, time, 1920); }
if(melody[i]==62 | melody[i]==65) { add_note(41, 64, 0, time, 1920); }
if(melody[i]==67) { add_note(51, 64, 0, time, 1920); }
if(melody[i]==68) { add_note(53, 64, 0, time, 1920); }
if(melody[i]==70) { add_note(55, 64, 0, time, 1920); }
if(melody[i]==72) { add_note(56, 64, 0, time, 1920); }
if(melody[i]==74) { add_note(58, 64, 0, time, 1920); }
if(melody[i]==72) { add_note(55, 64, 0, time, 1920); }
if(melody[i]==75) { add_note(60, 64, 0, time, 1920); }
if(melody[i]==75 | melody[i]==77) { add_note(53, 64, 0, time, 1920); }
if(melody[i]==79) { add_note(63, 64, 0, time, 1920); }
if(melody[i]==80) { add_note(65, 64, 0, time, 1920); }
if(melody[i]==82) { add_note(67, 64, 0, time, 1920); }
if(melody[i]==84) { add_note(68, 64, 0, time, 1920); }
}

}
save_score("3_Tenor.mid");



new_score();
for(i=0; i<TOTnotes; i++)
{
time = time + FINALrythms[i];

if(time==0 | time%1920==0) 
{ 
if(melody[i]==60 | melody[i]==63) { add_note(36, 64, 0, time, 1920); }
if(melody[i]==62 | melody[i]==65) { add_note(34, 64, 0, time, 1920); }
if(melody[i]==67) { add_note(39, 64, 0, time, 1920); }
if(melody[i]==68) { add_note(41, 64, 0, time, 1920); }
if(melody[i]==70) { add_note(43, 64, 0, time, 1920); }
if(melody[i]==72) { add_note(44, 64, 0, time, 1920); }
if(melody[i]==74) { add_note(46, 64, 0, time, 1920); }
if(melody[i]==75) { add_note(48, 64, 0, time, 1920); }
if(melody[i]==79) { add_note(41, 64, 0, time, 1920); }
if(melody[i]==80) { add_note(43, 64, 0, time, 1920); }
if(melody[i]==82) { add_note(55, 64, 0, time, 1920); }
if(melody[i]==84) { add_note(56, 64, 0, time, 1920); }
}

}
save_score("4_Bass.mid");

}



int main()
{

int numBARS;
int n;

printf("Welcome to Simple Melody Generator (4-part, short-melodic content, melancholic, C min 4/4 Version)! \n\n ");
printf("Please input number of total bars (integer number only) and press enter: ");
scanf("%d", &n);

numBARS = n;

/* 2dim array: numbBARS * 16 (all16hts) */
int rythms[numBARS][16];

/*initialize dummy indeces*/
int i, j;

/* initialize array */
for(i=0; i<numBARS; i++)
{

	for(j=0; j<16; j++)
	{
	rythms[i][j]=0;
	}

}

int rythmSUM=0;

int rythmweights[10]={ 120, 240, 240, 240, 480, 480, 480, 480, 480, 960};

/*generate random numer */
srand(time(NULL));

for(i=0; i<numBARS;i++)

{

 	for(j=0; j<16; j++)
	{
	
	int r = rand()%10;
	
	rythms[i][j]= rythmweights[r];
	rythmSUM = rythmSUM + rythmweights[r];
	
	/*constraint on rythms*/
	if(rythms[i][0]==120)
	{
	j=j-1;
	rythmSUM = rythmSUM - rythmweights[r];
	}
	
	
	if(rythmSUM==1920)
	{
	j=16;
	}
	
	if(rythmSUM>1920)
	{
	j=j-1;
	rythmSUM = rythmSUM - rythmweights[r];
	}

	}
	
	rythmSUM=0;
	
}


/*count all */
int totalCOUNT=0;

for(i=0; i<numBARS; i++)
	{
		for(j=0;j<16;j++)
		{
		if(rythms[i][j] != 0)
		{
	totalCOUNT++;
		}

		}

}



int FINALrythms[totalCOUNT];

	int k =0;

for(i=0; i<numBARS; i++)
	{
		for(j=0;j<16;j++)
		{
		if(rythms[i][j] != 0)
		{
		FINALrythms[k]=rythms[i][j];
		k++;
		}

}
}






/*	HERE COUNTERPOINT */
int notes[15]={60, 62, 63, 65, 67, 68, 70, 72, 74, 75, 77, 79, 80, 82, 84};

/*count total number of rythms generated, that's how many notes you need */

int TOTnotes = totalCOUNT;
int melody[TOTnotes];

int pick;

for(i=0; i<TOTnotes; i++)
{

int r=rand()%15;
pick=notes[r];

melody[i]=notes[r];

int k = cprules(i, melody);
i=k;

}


createmusic( FINALrythms, melody, TOTnotes);

printf("\n\n Music created successfully! Please check your native folder for your MIDI files. \n\n");

return 0;


}