#ifndef RIGCTL_H
#define RIGCTL_H

struct _SERIALPORT {
  //
  // parity and bits are not included, since we
  // always use 8 bits and "no parity"
  //
  char port[64];    // e.g. "/dev/ttyACM0"
  int  baud;        // baud rate
  int  enable;      // is it enabled?
#ifdef ANDROMEDA
  int  andromeda;   // flag for handling ANDROMEDA console
#endif
};

typedef struct _SERIALPORT SERIALPORT;

#define MAX_SERIAL 2
extern SERIALPORT SerialPorts[2];
extern gboolean rigctl_debug;

void launch_rigctl (void);
int launch_serial (int id);
void launch_andromeda (int id);
void disable_serial (int id);
void disable_andromeda (int id);

void  close_rigctl_ports (void);
int   rigctlGetMode(void);
int   lookup_band(int);
char * rigctlGetFilter(void);
void set_freqB(long long);
extern int cat_control;
int set_alc(gpointer);
extern int rigctl_busy;

extern int rigctl_port_base;
extern int rigctl_enable;


#endif // RIGCTL_H
