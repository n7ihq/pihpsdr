/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <stdio.h>
#include <stdlib.h>

#include "sliders.h"
#include "filter.h"
#include "receiver.h"
#include "vfo.h"
#include "radio.h"
#include "property.h"
#include "actions.h"
#include "mode.h"

//
// mode-specific defaults for the Var1 and Var2 filters
// These are now stored separately to allow for a
// "set variable filters to default" action
//
#define  LSB_VAR1_DEFAULT_LOW  -2850
#define  LSB_VAR1_DEFAULT_HIGH  -150
#define  LSB_VAR2_DEFAULT_LOW  -2850
#define  LSB_VAR2_DEFAULT_HIGH  -150
#define DIGL_VAR1_DEFAULT_LOW  -3000
#define DIGL_VAR1_DEFAULT_HIGH     0
#define DIGL_VAR2_DEFAULT_LOW  -2000
#define DIGL_VAR2_DEFAULT_HIGH -1000
#define  USB_VAR1_DEFAULT_LOW    150
#define  USB_VAR1_DEFAULT_HIGH  2850
#define  USB_VAR2_DEFAULT_LOW    150
#define  USB_VAR2_DEFAULT_HIGH  2850
#define DIGU_VAR1_DEFAULT_LOW      0
#define DIGU_VAR1_DEFAULT_HIGH  3000
#define DIGU_VAR2_DEFAULT_LOW   1000
#define DIGU_VAR2_DEFAULT_HIGH  2000
#define  CWL_VAR1_DEFAULT_LOW   -125
#define  CWL_VAR1_DEFAULT_HIGH   125
#define  CWL_VAR2_DEFAULT_LOW   -250
#define  CWL_VAR2_DEFAULT_HIGH   250
#define  CWU_VAR1_DEFAULT_LOW   -125
#define  CWU_VAR1_DEFAULT_HIGH   125
#define  CWU_VAR2_DEFAULT_LOW   -250
#define  CWU_VAR2_DEFAULT_HIGH   250
#define   AM_VAR1_DEFAULT_LOW  -3300
#define   AM_VAR1_DEFAULT_HIGH  3300
#define   AM_VAR2_DEFAULT_LOW  -3300
#define   AM_VAR2_DEFAULT_HIGH  3300
#define  SAM_VAR1_DEFAULT_LOW  -3300
#define  SAM_VAR1_DEFAULT_HIGH  3300
#define  SAM_VAR2_DEFAULT_LOW  -3300
#define  SAM_VAR2_DEFAULT_HIGH  3300
#define  FMN_VAR1_DEFAULT_LOW  -3300
#define  FMN_VAR1_DEFAULT_HIGH  3300
#define  FMN_VAR2_DEFAULT_LOW  -3300
#define  FMN_VAR2_DEFAULT_HIGH  3300
#define  DSB_VAR1_DEFAULT_LOW  -3300
#define  DSB_VAR1_DEFAULT_HIGH  3300
#define  DSB_VAR2_DEFAULT_LOW  -3300
#define  DSB_VAR2_DEFAULT_HIGH  3300
#define SPEC_VAR1_DEFAULT_LOW  -3300
#define SPEC_VAR1_DEFAULT_HIGH  3300
#define SPEC_VAR2_DEFAULT_LOW  -3300
#define SPEC_VAR2_DEFAULT_HIGH  3300
#define  DRM_VAR1_DEFAULT_LOW  -3300
#define  DRM_VAR1_DEFAULT_HIGH  3300
#define  DRM_VAR2_DEFAULT_LOW  -3300
#define  DRM_VAR2_DEFAULT_HIGH  3300

static FILTER filterLSB[FILTERS]={
    {-5150,-150,"5.0k"},
    {-4550,-150,"4.4k"},
    {-3950,-150,"3.8k"},
    {-3450,-150,"3.3k"},
    {-3050,-150,"2.9k"},
    {-2850,-150,"2.7k"},
    {-2550,-150,"2.4k"},
    {-2250,-150,"2.1k"},
    {-1950,-150,"1.8k"},
    {-1150,-150,"1.0k"},
    {LSB_VAR1_DEFAULT_LOW,LSB_VAR1_DEFAULT_HIGH,"Var1"},
    {LSB_VAR2_DEFAULT_LOW,LSB_VAR2_DEFAULT_HIGH,"Var2"}
    };

static FILTER filterUSB[FILTERS]={
    {150,5150,"5.0k"},
    {150,4550,"4.4k"},
    {150,3950,"3.8k"},
    {150,3450,"3.3k"},
    {150,3050,"2.9k"},
    {150,2850,"2.7k"},
    {150,2550,"2.4k"},
    {150,2250,"2.1k"},
    {150,1950,"1.8k"},
    {150,1150,"1.0k"},
    {USB_VAR1_DEFAULT_LOW,USB_VAR1_DEFAULT_HIGH,"Var1"},
    {USB_VAR2_DEFAULT_LOW,USB_VAR2_DEFAULT_HIGH,"Var2"}
    };

//
// DigiMode Filters up to 3000 Hz wide are centered
// around 1500, the broader ones start at
// zero (this also holds for DIGU).
//
static FILTER filterDIGL[FILTERS]={
    {-5000,    0,"5.0k"},
    {-4000,    0,"4.0k"},
    {-3000,    0,"3.0k"},
    {-2750, -250,"2.5k"},
    {-2500, -500,"2.0k"},
    {-2250, -750,"1.5k"},
    {-2000,-1000,"1.0k"},
    {-1875,-1125,"750"},
    {-1750,-1250,"500"},
    {-1625,-1375,"250"},
    {DIGL_VAR1_DEFAULT_LOW,DIGL_VAR1_DEFAULT_HIGH,"Var1"},
    {DIGL_VAR2_DEFAULT_LOW,DIGL_VAR2_DEFAULT_HIGH,"Var2"}
    };

static FILTER filterDIGU[FILTERS]={
    {   0,5000,"5.0k"},
    {   0,4000,"4.0k"},
    {   0,3000,"3.0k"},
    { 250,2750,"2.5k"},
    { 500,2500,"2.0k"},
    { 750,2250,"1.5k"},
    {1000,2000,"1.0k"},
    {1125,1875,"750"},
    {1250,1750,"500"},
    {1375,1625,"250"},
    {DIGU_VAR1_DEFAULT_LOW,DIGU_VAR1_DEFAULT_HIGH,"Var1"},
    {DIGU_VAR2_DEFAULT_LOW,DIGU_VAR2_DEFAULT_HIGH,"Var2"}
    };

//
// CW filter edges refer to a CW signal at zero frequency
//
static FILTER filterCWL[FILTERS]={
    {-500,500,"1.0k"},
    {-400,400,"800"},
    {-375,375,"750"},
    {-300,300,"600"},
    {-250,250,"500"},
    {-200,200,"400"},
    {-125,125,"250"},
    {-50,50,"100"},
    {-25,25,"50"},
    {-13,13,"25"},
    {CWL_VAR1_DEFAULT_LOW,CWL_VAR1_DEFAULT_HIGH,"Var1"},
    {CWL_VAR2_DEFAULT_LOW,CWL_VAR2_DEFAULT_HIGH,"Var2"}
    };

static FILTER filterCWU[FILTERS]={
    {-500,500,"1.0k"},
    {-400,400,"800"},
    {-375,375,"750"},
    {-300,300,"600"},
    {-250,250,"500"},
    {-200,200,"400"},
    {-125,125,"250"},
    {-50,50,"100"},
    {-25,25,"50"},
    {-13,13,"25"},
    {CWU_VAR1_DEFAULT_LOW,CWU_VAR1_DEFAULT_HIGH,"Var1"},
    {CWU_VAR2_DEFAULT_LOW,CWU_VAR2_DEFAULT_HIGH,"Var2"}
    };

//
// DSB, AM, SAM, SPEC and DRM  filters normally have low/high edges
// that only differ in sign
//
static FILTER filterDSB[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {DSB_VAR1_DEFAULT_LOW,DSB_VAR1_DEFAULT_HIGH,"Var1"},
    {DSB_VAR2_DEFAULT_LOW,DSB_VAR2_DEFAULT_HIGH,"Var2"}
    };

static FILTER filterAM[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {AM_VAR1_DEFAULT_LOW,AM_VAR1_DEFAULT_HIGH,"Var1"},
    {AM_VAR2_DEFAULT_LOW,AM_VAR2_DEFAULT_HIGH,"Var2"}
    };

static FILTER filterSAM[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {SAM_VAR1_DEFAULT_LOW,SAM_VAR1_DEFAULT_HIGH,"Var1"},
    {SAM_VAR2_DEFAULT_LOW,SAM_VAR2_DEFAULT_HIGH,"Var2"}
    };

static FILTER filterSPEC[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {SPEC_VAR1_DEFAULT_LOW,SPEC_VAR1_DEFAULT_HIGH,"Var1"},
    {SPEC_VAR2_DEFAULT_LOW,SPEC_VAR2_DEFAULT_HIGH,"Var2"}
    };

static FILTER filterDRM[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {DRM_VAR1_DEFAULT_LOW,DRM_VAR1_DEFAULT_HIGH,"Var1"},
    {DRM_VAR2_DEFAULT_LOW,DRM_VAR2_DEFAULT_HIGH,"Var2"}
    };

//
// This FMN filter edges are nowhere used, this data is
// just there to avoid voids. The FMN filter edges are
// instead determined from the FM deviation
//
static FILTER filterFMN[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {FMN_VAR1_DEFAULT_LOW,FMN_VAR1_DEFAULT_HIGH,"Var1"},
    {FMN_VAR2_DEFAULT_LOW,FMN_VAR2_DEFAULT_HIGH,"Var2"}
    };

//
// The filters in this list must be in exactly the same
// order as the modes in enum mode_list (see mode.h)!
//
FILTER *filters[MODES]={
    filterLSB,
    filterUSB,
    filterDSB,
    filterCWL,
    filterCWU,
    filterFMN,
    filterAM,
    filterDIGU,
    filterSPEC,
    filterDIGL,
    filterSAM,
    filterDRM
};

//
// These arrays contain the default low/high filter edges
// for the Var1/Var2 filters for each mode.
// There is now a "set default" action that restores the
// default.
// The order of modes must be exactly as in the mode_list enum.
//
const int var1_default_low[MODES]={
  LSB_VAR1_DEFAULT_LOW,
  USB_VAR1_DEFAULT_LOW,
  DSB_VAR1_DEFAULT_LOW,
  CWL_VAR1_DEFAULT_LOW,
  CWU_VAR1_DEFAULT_LOW,
  FMN_VAR1_DEFAULT_LOW,
  AM_VAR1_DEFAULT_LOW,
  DIGU_VAR1_DEFAULT_LOW,
  SPEC_VAR1_DEFAULT_LOW,
  DIGL_VAR1_DEFAULT_LOW,
  SAM_VAR1_DEFAULT_LOW,
  DRM_VAR1_DEFAULT_LOW
};

const int var1_default_high[MODES]={
  LSB_VAR1_DEFAULT_HIGH,
  USB_VAR1_DEFAULT_HIGH,
  DSB_VAR1_DEFAULT_HIGH,
  CWL_VAR1_DEFAULT_HIGH,
  CWU_VAR1_DEFAULT_HIGH,
  FMN_VAR1_DEFAULT_HIGH,
  AM_VAR1_DEFAULT_HIGH,
  DIGU_VAR1_DEFAULT_HIGH,
  SPEC_VAR1_DEFAULT_HIGH,
  DIGL_VAR1_DEFAULT_HIGH,
  SAM_VAR1_DEFAULT_HIGH,
  DRM_VAR1_DEFAULT_HIGH
};

const int var2_default_low[MODES]={
  LSB_VAR2_DEFAULT_LOW,
  USB_VAR2_DEFAULT_LOW,
  DSB_VAR2_DEFAULT_LOW,
  CWL_VAR2_DEFAULT_LOW,
  CWU_VAR2_DEFAULT_LOW,
  FMN_VAR2_DEFAULT_LOW,
  AM_VAR2_DEFAULT_LOW,
  DIGU_VAR2_DEFAULT_LOW,
  SPEC_VAR2_DEFAULT_LOW,
  DIGL_VAR2_DEFAULT_LOW,
  SAM_VAR2_DEFAULT_LOW,
  DRM_VAR2_DEFAULT_LOW
};

const int var2_default_high[MODES]={
  LSB_VAR2_DEFAULT_HIGH,
  USB_VAR2_DEFAULT_HIGH,
  DSB_VAR2_DEFAULT_HIGH,
  CWL_VAR2_DEFAULT_HIGH,
  CWU_VAR2_DEFAULT_HIGH,
  FMN_VAR2_DEFAULT_HIGH,
  AM_VAR2_DEFAULT_HIGH,
  DIGU_VAR2_DEFAULT_HIGH,
  SPEC_VAR2_DEFAULT_HIGH,
  DIGL_VAR2_DEFAULT_HIGH,
  SAM_VAR2_DEFAULT_HIGH,
  DRM_VAR2_DEFAULT_HIGH
};

void filterSaveState() {
    char value[128];

    // save the Var1 and Var2 settings
    sprintf(value,"%d",filterLSB[filterVar1].low);
    setProperty("filter.lsb.var1.low",value);
    sprintf(value,"%d",filterLSB[filterVar1].high);
    setProperty("filter.lsb.var1.high",value);
    sprintf(value,"%d",filterLSB[filterVar2].low);
    setProperty("filter.lsb.var2.low",value);
    sprintf(value,"%d",filterLSB[filterVar2].high);
    setProperty("filter.lsb.var2.high",value);

    sprintf(value,"%d",filterDIGL[filterVar1].low);
    setProperty("filter.digl.var1.low",value);
    sprintf(value,"%d",filterDIGL[filterVar1].high);
    setProperty("filter.digl.var1.high",value);
    sprintf(value,"%d",filterDIGL[filterVar2].low);
    setProperty("filter.digl.var2.low",value);
    sprintf(value,"%d",filterDIGL[filterVar2].high);
    setProperty("filter.digl.var2.high",value);

    sprintf(value,"%d",filterCWL[filterVar1].low);
    setProperty("filter.cwl.var1.low",value);
    sprintf(value,"%d",filterCWL[filterVar1].high);
    setProperty("filter.cwl.var1.high",value);
    sprintf(value,"%d",filterCWL[filterVar2].low);
    setProperty("filter.cwl.var2.low",value);
    sprintf(value,"%d",filterCWL[filterVar2].high);
    setProperty("filter.cwl.var2.high",value);

    sprintf(value,"%d",filterUSB[filterVar1].low);
    setProperty("filter.usb.var1.low",value);
    sprintf(value,"%d",filterUSB[filterVar1].high);
    setProperty("filter.usb.var1.high",value);
    sprintf(value,"%d",filterUSB[filterVar2].low);
    setProperty("filter.usb.var2.low",value);
    sprintf(value,"%d",filterUSB[filterVar2].high);
    setProperty("filter.usb.var2.high",value);

    sprintf(value,"%d",filterDIGU[filterVar1].low);
    setProperty("filter.digu.var1.low",value);
    sprintf(value,"%d",filterDIGU[filterVar1].high);
    setProperty("filter.digu.var1.high",value);
    sprintf(value,"%d",filterDIGU[filterVar2].low);
    setProperty("filter.digu.var2.low",value);
    sprintf(value,"%d",filterDIGU[filterVar2].high);
    setProperty("filter.digu.var2.high",value);

    sprintf(value,"%d",filterCWU[filterVar1].low);
    setProperty("filter.cwu.var1.low",value);
    sprintf(value,"%d",filterCWU[filterVar1].high);
    setProperty("filter.cwu.var1.high",value);
    sprintf(value,"%d",filterCWU[filterVar2].low);
    setProperty("filter.cwu.var2.low",value);
    sprintf(value,"%d",filterCWU[filterVar2].high);
    setProperty("filter.cwu.var2.high",value);

    sprintf(value,"%d",filterAM[filterVar1].low);
    setProperty("filter.am.var1.low",value);
    sprintf(value,"%d",filterAM[filterVar1].high);
    setProperty("filter.am.var1.high",value);
    sprintf(value,"%d",filterAM[filterVar2].low);
    setProperty("filter.am.var2.low",value);
    sprintf(value,"%d",filterAM[filterVar2].high);
    setProperty("filter.am.var2.high",value);

    sprintf(value,"%d",filterSAM[filterVar1].low);
    setProperty("filter.sam.var1.low",value);
    sprintf(value,"%d",filterSAM[filterVar1].high);
    setProperty("filter.sam.var1.high",value);
    sprintf(value,"%d",filterSAM[filterVar2].low);
    setProperty("filter.sam.var2.low",value);
    sprintf(value,"%d",filterSAM[filterVar2].high);
    setProperty("filter.sam.var2.high",value);

    sprintf(value,"%d",filterFMN[filterVar1].low);
    setProperty("filter.fmn.var1.low",value);
    sprintf(value,"%d",filterFMN[filterVar1].high);
    setProperty("filter.fmn.var1.high",value);
    sprintf(value,"%d",filterFMN[filterVar2].low);
    setProperty("filter.fmn.var2.low",value);
    sprintf(value,"%d",filterFMN[filterVar2].high);
    setProperty("filter.fmn.var2.high",value);

    sprintf(value,"%d",filterDSB[filterVar1].low);
    setProperty("filter.dsb.var1.low",value);
    sprintf(value,"%d",filterDSB[filterVar1].high);
    setProperty("filter.dsb.var1.high",value);
    sprintf(value,"%d",filterDSB[filterVar2].low);
    setProperty("filter.dsb.var2.low",value);
    sprintf(value,"%d",filterDSB[filterVar2].high);
    setProperty("filter.dsb.var2.high",value);

}

void filterRestoreState() {
    char* value;

    value=getProperty("filter.lsb.var1.low");
    if(value) filterLSB[filterVar1].low=atoi(value);
    value=getProperty("filter.lsb.var1.high");
    if(value) filterLSB[filterVar1].high=atoi(value);
    value=getProperty("filter.lsb.var2.low");
    if(value) filterLSB[filterVar2].low=atoi(value);
    value=getProperty("filter.lsb.var2.high");
    if(value) filterLSB[filterVar2].high=atoi(value);

    value=getProperty("filter.digl.var1.low");
    if(value) filterDIGL[filterVar1].low=atoi(value);
    value=getProperty("filter.digl.var1.high");
    if(value) filterDIGL[filterVar1].high=atoi(value);
    value=getProperty("filter.digl.var2.low");
    if(value) filterDIGL[filterVar2].low=atoi(value);
    value=getProperty("filter.digl.var2.high");
    if(value) filterDIGL[filterVar2].high=atoi(value);

    value=getProperty("filter.cwl.var1.low");
    if(value) filterCWL[filterVar1].low=atoi(value);
    value=getProperty("filter.cwl.var1.high");
    if(value) filterCWL[filterVar1].high=atoi(value);
    value=getProperty("filter.cwl.var2.low");
    if(value) filterCWL[filterVar2].low=atoi(value);
    value=getProperty("filter.cwl.var2.high");
    if(value) filterCWL[filterVar2].high=atoi(value);

    value=getProperty("filter.usb.var1.low");
    if(value) filterUSB[filterVar1].low=atoi(value);
    value=getProperty("filter.usb.var1.high");
    if(value) filterUSB[filterVar1].high=atoi(value);
    value=getProperty("filter.usb.var2.low");
    if(value) filterUSB[filterVar2].low=atoi(value);
    value=getProperty("filter.usb.var2.high");
    if(value) filterUSB[filterVar2].high=atoi(value);

    value=getProperty("filter.digu.var1.low");
    if(value) filterDIGU[filterVar1].low=atoi(value);
    value=getProperty("filter.digu.var1.high");
    if(value) filterDIGU[filterVar1].high=atoi(value);
    value=getProperty("filter.digu.var2.low");
    if(value) filterDIGU[filterVar2].low=atoi(value);
    value=getProperty("filter.digu.var2.high");
    if(value) filterDIGU[filterVar2].high=atoi(value);

    value=getProperty("filter.cwu.var1.low");
    if(value) filterCWU[filterVar1].low=atoi(value);
    value=getProperty("filter.cwu.var1.high");
    if(value) filterCWU[filterVar1].high=atoi(value);
    value=getProperty("filter.cwu.var2.low");
    if(value) filterCWU[filterVar2].low=atoi(value);
    value=getProperty("filter.cwu.var2.high");
    if(value) filterCWU[filterVar2].high=atoi(value);

    value=getProperty("filter.am.var1.low");
    if(value) filterAM[filterVar1].low=atoi(value);
    value=getProperty("filter.am.var1.high");
    if(value) filterAM[filterVar1].high=atoi(value);
    value=getProperty("filter.am.var2.low");
    if(value) filterAM[filterVar2].low=atoi(value);
    value=getProperty("filter.am.var2.high");
    if(value) filterAM[filterVar2].high=atoi(value);

    value=getProperty("filter.sam.var1.low");
    if(value) filterSAM[filterVar1].low=atoi(value);
    value=getProperty("filter.sam.var1.high");
    if(value) filterSAM[filterVar1].high=atoi(value);
    value=getProperty("filter.sam.var2.low");
    if(value) filterSAM[filterVar2].low=atoi(value);
    value=getProperty("filter.sam.var2.high");
    if(value) filterSAM[filterVar2].high=atoi(value);

    value=getProperty("filter.fmn.var1.low");
    if(value) filterFMN[filterVar1].low=atoi(value);
    value=getProperty("filter.fmn.var1.high");
    if(value) filterFMN[filterVar1].high=atoi(value);
    value=getProperty("filter.fmn.var2.low");
    if(value) filterFMN[filterVar2].low=atoi(value);
    value=getProperty("filter.fmn.var2.high");
    if(value) filterFMN[filterVar2].high=atoi(value);

    value=getProperty("filter.dsb.var1.low");
    if(value) filterDSB[filterVar1].low=atoi(value);
    value=getProperty("filter.dsb.var1.high");
    if(value) filterDSB[filterVar1].high=atoi(value);
    value=getProperty("filter.dsb.var2.low");
    if(value) filterDSB[filterVar2].low=atoi(value);
    value=getProperty("filter.dsb.var2.high");
    if(value) filterDSB[filterVar2].high=atoi(value);

}

//
// This function is a no-op unless the vfo referenced uses a Var1 or Var2 filter
//
void filter_cut_default(int id) {
  int mode=vfo[id].mode;
  int f=vfo[id].filter;
  FILTER *filter=&(filters[mode][f]);

  switch (f) {
    case filterVar1:
      filter->low =var1_default_low [mode];
      filter->high=var1_default_high[mode];
      break;
    case filterVar2:
      filter->low =var2_default_low [mode];
      filter->high=var2_default_high[mode];
      break;
    default:
      // do nothing
      break;
  }
g_print("%s: mode=%d filter=%d low=%d high=%d\n",__FUNCTION__,mode,f,filter->low,filter->high);
  vfo_filter_changed(f);
}
//
// This function is a no-op unless the vfo referenced uses a Var1 or Var2 filter
//
void filter_cut_changed(int id, int action, int increment) {
  int mode=vfo[id].mode;
  int f=vfo[id].filter;
  FILTER *filter=&(filters[mode][f]);
  int st = (mode == modeCWU || mode == modeCWL) ? 2 : 5;

  if(f==filterVar1 || f==filterVar2) {

    switch(action) {
      case FILTER_CUT_HIGH:
        filter->high += increment*st;
        set_filter_cut_high(id,filter->high);
        break;
      case FILTER_CUT_LOW:
        filter->low += increment*st;
        set_filter_cut_low(id,filter->low);
        break;
      default:
        break;
    }
    vfo_filter_changed(f);
g_print("%s: rx=%d action=%d, mode=%d filter=%d low=%d high=%d\n", __FUNCTION__,id,action,mode,f,filter->low, filter->high);
  }
}

//
// This function is a no-op unless the vfo referenced uses a Var1 or Var2 filter
//
void filter_width_changed(int id,int increment) {
  int mode=vfo[id].mode;
  int f=vfo[id].filter;
  FILTER *filter=&(filters[mode][f]);

  if(f==filterVar1 || f==filterVar2) {

    switch(mode) {
      case modeLSB:
      case modeDIGL:
        filter->low -= increment*5;
        break;
      case modeUSB:
      case modeDIGU:
        filter->high += increment*5;
        break;
      case modeCWL:
      case modeCWU:
        filter->low  -= increment*2;
        filter->high += increment*2;
        break;
      default:
        filter->low  -= increment*5;
        filter->high += increment*5;
        break;
    }
    vfo_filter_changed(f);
    set_filter_width(id,filter->high-filter->low);
g_print("%s: rx=%d mode=%d filter=%d low=%d high=%d\n",__FUNCTION__,id,vfo[id].mode,vfo[id].filter,filter->low,filter->high);
  }
}

//
// This function is a no-op unless the vfo referenced uses a Var1 or Var2 filter
//
void filter_shift_changed(int id,int increment) {
  int mode=vfo[id].mode;
  int f=vfo[id].filter;
  FILTER *filter=&(filters[mode][f]);

  if(f==filterVar1 || f==filterVar2) {
    switch(mode) {
      case modeLSB:
      case modeDIGL:
        filter->low=filter->low+(increment*5);
        filter->high=filter->high+(increment*5);
        set_filter_shift(id,filter->high);
        break;
      case modeUSB:
      case modeDIGU:
        filter->low=filter->low+(increment*5);
        filter->high=filter->high+(increment*5);
        set_filter_shift(id,filter->low);
        break;

      default:
        // shifting only useful for "single side band" modes.
        break;
    }
    vfo_filter_changed(f),
g_print("%s: rx=%d mode=%d filter=%d low=%d high=%d\n",__FUNCTION__,id,vfo[id].mode,vfo[id].filter,filter->low,filter->high);
  }

}
