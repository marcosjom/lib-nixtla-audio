CFLAGS=-I../../../include
LDFLAGS=-L.. -lnixtla-audio -lOpenAL -L/Applications/Momentics.app/target_10_3_1_995/qnx6/x86/usr/lib
all: demoCaptureEco demoPlayWav
demoPlayWav: utilLoadWav.o ../libnixtla-audio.so demoPlayWav.o
	$(CC) $(LDFLAGS) demoPlayWav.o ../libnixtla-audio.so -o demoPlayWav
