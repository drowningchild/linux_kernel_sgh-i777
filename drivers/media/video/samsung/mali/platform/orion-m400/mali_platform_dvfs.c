/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform_dvfs.c
 * Platform specific Mali driver dvfs functions
 */

#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>

#include <asm/io.h>

#ifdef CONFIG_S5PV310_ASV
#include <mach/asv.h>
#endif

#include "mali_device_pause_resume.h"
#include <linux/workqueue.h>

#define MALI_DVFS_STEPS 3 // 4
#define MALI_DVFS_WATING 10 // msec

#define MALI_DVFS_CLK_DEBUG 0
#define MALI_CLK_VERIFICATION 0
#define MALI_DVFS_PAUSE_RESUME_TEST 0

#if MALI_CLK_VERIFICATION
#define NUM_OF_TEST_LOOP 2
#endif

static int bMaliDvfsRun=0;

typedef struct mali_dvfs_tableTag{
    unsigned int clock;
    unsigned int freq;
    unsigned int vol;
}mali_dvfs_table;

typedef struct mali_dvfs_statusTag{
    unsigned int currentStep;
    mali_dvfs_table * pCurrentDvfs;

}mali_dvfs_status;

/*dvfs status*/
mali_dvfs_status maliDvfsStatus;
int mali_dvfs_control=0;

/*dvfs table*/
typedef struct mali_dvfs_thresholdTag{
	unsigned int downthreshold;
	unsigned int upthreshold;
}mali_dvfs_threshold_table;

typedef struct mali_dvfs_staycount{
	unsigned int staycount;
}mali_dvfs_staycount_table;

mali_dvfs_staycount_table mali_dvfs_staycount[MALI_DVFS_STEPS]={
		/*step 0*/{1},
		/*step 1*/{1},
		/*step 2*/{1} };

/*
  dvfs threshold
*/

mali_dvfs_threshold_table mali_dvfs_threshold[MALI_DVFS_STEPS]={
		/*step 0*/{((int)((255*0)/100))   ,((int)((255*85)/100))},
		/*step 1*/{((int)((255*25)/100))  ,((int)((255*85)/100))},
		/*step 2*/{((int)((255*25)/100))  ,((int)((255*100)/100))}
};

mali_dvfs_table mali_dvfs[MALI_DVFS_STEPS]={
#ifdef CONFIG_S5PV310_ASV
			/*step 0*/{66  ,1000000    ,900000},
			/*step 1*/{160  ,1000000    ,950000},
			/*step 2*/{267  ,1000000    ,1000000}
};
#else
			/*step 0*/{66  ,1000000    , 900000},
			/*step 1*/{160  ,1000000    ,950000},
			/*step 2*/{267  ,1000000    ,1000000}
};
#endif

#ifdef CONFIG_S5PV310_ASV
#define ASV_8_LEVEL	8
#define ASV_5_LEVEL	5

static unsigned int asv_3d_volt_5_table[ASV_5_LEVEL][MALI_DVFS_STEPS] = {
	/* L3(66MHz), L2(160MHz), L1(267MHz) */
	{ 950000, 1000000, 1100000},	/* S */
	{ 950000, 1000000, 1100000},	/* A */
	{ 950000,  950000, 1000000},	/* B */
	{ 900000,  950000, 1000000},	/* C */
	{ 900000,  950000,  950000},	/* D */
};

static unsigned int asv_3d_volt_8_table[ASV_8_LEVEL][MALI_DVFS_STEPS] = {
	/* L3(66MHz), L2(160MHz)), L1(267MHz) */
	{ 950000, 1000000, 1100000},	/* SS */
	{ 950000, 1000000, 1100000},	/* A1 */
	{ 950000, 1000000, 1100000},	/* A2 */
	{ 900000,  950000, 1000000},	/* B1 */
	{ 900000,  950000, 1000000},	/* B2 */
	{ 900000,  950000, 1000000},	/* C1 */
	{ 900000,  950000, 1000000},	/* C2 */
	{ 900000,  950000, 1000000},	/* D1 */
};
#endif

static u32 mali_dvfs_utilization = 255;

static void mali_dvfs_work_handler(struct work_struct *w);

static struct workqueue_struct *mali_dvfs_wq = 0;
extern mali_io_address clk_register_map;

static DECLARE_WORK(mali_dvfs_work, mali_dvfs_work_handler);

static unsigned int get_mali_dvfs_staus(void)
{

#ifdef CONFIG_REGULATOR
#if MALI_CLK_VERIFICATION
    unsigned long clk_rate=0;
    int voltage=0;
#endif
#endif

#if MALI_CLK_VERIFICATION
    int stepIndex=0;
    unsigned int testLoop=NUM_OF_TEST_LOOP;
#endif

    /*set extra parameters here in the future
    */

#if MALI_CLK_VERIFICATION
    while(testLoop--)
    {

        /*loop tests for avoiding fluctuation*/

        /*get current clk rate and voltage*/
        clk_rate = mali_clk_get_rate();
        voltage = regulator_get_voltage(g3d_regulator);

        for(stepIndex=0;stepIndex<MALI_DVFS_STEPS;stepIndex++)
        {
            if(mali_dvfs[stepIndex].vol ==voltage)
            {
                if(mali_dvfs[stepIndex].clock == clk_rate/mali_dvfs[stepIndex].freq)
                {
                    maliDvfsStatus.currentStep=stepIndex;
                    maliDvfsStatus.pCurrentDvfs=&mali_dvfs[stepIndex];
                    return maliDvfsStatus.currentStep;
                }
            }
        }
    }

    MALI_DEBUG_PRINT(1, ("[DVFS]invalid step in get-->reset to default step \n"));
    /*error handling for current status -> set default step*/
#ifdef CONFIG_REGULATOR
    /*change the voltage*/
    mali_regulator_set_voltage(mali_dvfs[MALI_DVFS_DEFAULT_STEP].vol, mali_dvfs[MALI_DVFS_DEFAULT_STEP].vol);
#endif
    /*change the clock*/
    mali_clk_set_rate(mali_dvfs[MALI_DVFS_DEFAULT_STEP].clock, mali_dvfs[MALI_DVFS_DEFAULT_STEP].freq);

	mali_clk_put();
	//clk_put(mali_parent_clock);
	//clk_put(mpll_clock);
	maliDvfsStatus.currentStep = MALI_DVFS_DEFAULT_STEP;
#endif /*MALI_CLK_VERIFICATION*/

	return maliDvfsStatus.currentStep;

}

static mali_bool set_mali_dvfs_staus(u32 step,mali_bool boostup)
{
    u32 validatedStep=step;
#ifdef CONFIG_REGULATOR
#if MALI_CLK_VERIFICATION
    unsigned long clk_rate=0;
    int voltage=0;
#endif
#endif
#if MALI_DVFS_CLK_DEBUG
    unsigned int *pRegMaliClkDiv;
    unsigned int *pRegMaliMpll;
#endif

#if MALI_CLK_VERIFICATION
    unsigned int testLoop=NUM_OF_TEST_LOOP;
#endif

#ifdef CONFIG_REGULATOR
#if 1
    if( mali_regulator_get_usecount()==0)
    {
        MALI_DEBUG_PRINT(1, ("regulator use_count is 0 \n"));
        return MALI_FALSE;
    }
#else
    int enabled=0;
    enabled = regulator_is_enabled(g3d_regulator);
    if(enabled < 0)
    {
        MALI_DEBUG_PRINT(1, ("regulator is enabled \n"));
    }
    else
    {
        mali_regulator_enable();
        MALI_DEBUG_PRINT(1, ("enable regulator\n"));
    }
#endif
#endif

    if(boostup)
    {
#ifdef CONFIG_REGULATOR
        /*change the voltage*/
        mali_regulator_set_voltage(mali_dvfs[step].vol, mali_dvfs[step].vol);
#endif
        /*change the clock*/
        mali_clk_set_rate(mali_dvfs[step].clock, mali_dvfs[step].freq);
    }
    else
    {
        /*change the clock*/
        mali_clk_set_rate(mali_dvfs[step].clock, mali_dvfs[step].freq);
#ifdef CONFIG_REGULATOR
        /*change the voltage*/
        mali_regulator_set_voltage(mali_dvfs[step].vol, mali_dvfs[step].vol);
#endif
    }

	mali_clk_put();
	//clk_put(mali_parent_clock);
	//clk_put(mpll_clock);
#if MALI_DVFS_CLK_DEBUG
    pRegMaliClkDiv = ioremap(0x1003c52c,32);
    pRegMaliMpll = ioremap(0x1003c22c,32);
    MALI_PRINT( ("Mali MPLL reg:%d, CLK DIV: %d \n",*pRegMaliMpll, *pRegMaliClkDiv));
#endif

#if MALI_CLK_VERIFICATION
    while(testLoop--)
    {
        /*loop tests for avoiding fluctuation*/

        clk_rate = mali_clk_get_rate();
        voltage = regulator_get_voltage(g3d_regulator);

        if((mali_dvfs[step].vol== voltage)||(mali_dvfs[step].clock== clk_rate/mali_dvfs[step].freq))
        {
            maliDvfsStatus.currentStep = validatedStep;
            /*for future use*/
            maliDvfsStatus.pCurrentDvfs = &mali_dvfs[validatedStep];
            return MALI_TRUE;
        }

    }

    MALI_DEBUG_PRINT(1, ("[DVFS]invalid step set dvfs -->reset to default step \n"));
    /*error handling for current status -> set default step*/
#ifdef CONFIG_REGULATOR
    /*change the voltage*/
    mali_regulator_set_voltage(mali_dvfs[MALI_DVFS_DEFAULT_STEP].vol, mali_dvfs[MALI_DVFS_DEFAULT_STEP].vol);
#endif
    /*change the clock*/
    mali_clk_set_rate(mali_dvfs[MALI_DVFS_DEFAULT_STEP].clock, mali_dvfs[MALI_DVFS_DEFAULT_STEP].freq);

    mali_clk_put();
    //clk_put(mali_parent_clock);
    //clk_put(mpll_clock);

    validatedStep = MALI_DVFS_DEFAULT_STEP;
#endif /*MALI_CLK_VERIFICATION*/

    maliDvfsStatus.currentStep = validatedStep;
    /*for future use*/
    maliDvfsStatus.pCurrentDvfs = &mali_dvfs[validatedStep];

    return MALI_TRUE;
}

static void mali_platform_wating(u32 msec)
{
    /*sample wating
    change this in the future with proper check routine.
    */
	unsigned int read_val;
	while(1)
	{
		read_val = _mali_osk_mem_ioread32(clk_register_map, 0x00);
		if ((read_val & 0x8000)==0x0000) break;

        _mali_osk_time_ubusydelay(100); // 1000 -> 100 : 20101218
	}
    /* _mali_osk_time_ubusydelay(msec*1000);*/
}

static mali_bool change_mali_dvfs_staus(u32 step, mali_bool boostup )
{

	MALI_DEBUG_PRINT(1, ("> change_mali_dvfs_staus: %d, %d \n",step, boostup));
#if MALI_DVFS_PAUSE_RESUME_TEST
	MALI_PRINT( ("> mali_dev_pause\n"));
	if(mali_dev_pause())
	{
		MALI_DEBUG_PRINT(1, ("error on mali_dev_dvfs_pause in change_mali_dvfs_staus"));
		return MALI_FALSE;
	}
	MALI_PRINT( ("< mali_dev_pause\n"));
#endif

    if(!set_mali_dvfs_staus(step, boostup))
    {
        MALI_DEBUG_PRINT(1, ("error on set_mali_dvfs_staus: %d, %d \n",step, boostup));
        return MALI_FALSE;
    }

    /*wait until clock and voltage is stablized*/
    mali_platform_wating(MALI_DVFS_WATING); /*msec*/

#if MALI_DVFS_PAUSE_RESUME_TEST
	MALI_PRINT( ("> mali_dev_resume\n"));
	if(mali_dev_resume())
	{
		MALI_DEBUG_PRINT(1, ("error on mali_dev_dvfs_resume in change_mali_dvfs_staus"));
		return MALI_FALSE;
	}
	MALI_PRINT( ("< mali_dev_resume\n"));
#endif
    return MALI_TRUE;
}

static unsigned int decideNextStatus(unsigned int utilization)
{
    unsigned int level=0; // 0:stay, 1:up
	if(!mali_dvfs_control)
	{
		switch(maliDvfsStatus.currentStep)
		{
			case 0:
				if( utilization > mali_dvfs_threshold[maliDvfsStatus.currentStep].upthreshold)
					level=1;
				else
					level = maliDvfsStatus.currentStep;
				break;
			case 1:
				if( utilization > mali_dvfs_threshold[maliDvfsStatus.currentStep].upthreshold)
					level=2;
				else if( utilization < mali_dvfs_threshold[maliDvfsStatus.currentStep].downthreshold)
					level=0;
				else
					level = maliDvfsStatus.currentStep;
				break;
			case 2:
				if( utilization < mali_dvfs_threshold[maliDvfsStatus.currentStep].downthreshold)
					level=1;
				else
					level = maliDvfsStatus.currentStep;
				break;
		}
	}
	else
	{
		if((mali_dvfs_control == 1)||(( mali_dvfs_control > 3) && (mali_dvfs_control < mali_dvfs[0].clock+1)))
		{
			level=0;
		}
		else if((mali_dvfs_control == 2)||(( mali_dvfs_control > mali_dvfs[0].clock) && (mali_dvfs_control < mali_dvfs[1].clock+1)))
		{
			level=1;
		}
		else
		{
			level=2;
		}
	}
    return level;
}

#ifdef CONFIG_S5PV310_ASV

extern struct s5pv310_asv_info asv_info;

static mali_bool mali_dvfs_table_update(void)
{
	unsigned int i;

		for (i = 0; i < MALI_DVFS_STEPS; i++) {
			mali_dvfs[i].vol = asv_3d_volt_8_table[asv_info.asv_num][i];
			printk(KERN_INFO "mali_dvfs[%d].vol = %d\n",
				i, mali_dvfs[i].vol);
		}

	return MALI_TRUE;

}
#endif

static mali_bool mali_dvfs_staus(u32 utilization)
{
	unsigned int nextStatus = 0;
	unsigned int curStatus = 0;
	mali_bool boostup = MALI_FALSE;
#ifdef CONFIG_S5PV310_ASV
	static mali_bool asv_applied = MALI_FALSE;
#endif
	static int stay_count = 0; // to prevent frequent switch

	MALI_DEBUG_PRINT(1, ("> mali_dvfs_staus: %d \n",utilization));
#ifdef CONFIG_S5PV310_ASV
	if (asv_applied == MALI_FALSE)
	{
		if (asv_info.asv_init_done == 1)
		{
			mali_dvfs_table_update();
			change_mali_dvfs_staus(0,0);
			asv_applied = MALI_TRUE;
			return MALI_TRUE; // first 3D DVFS with ASV -> just change the table and base setting.
		}
		else
		{
			return MALI_TRUE; // ignore 3D DVFS until ASV group number is ready.
		}
	}
#endif

#if 1
    /*decide next step*/
	curStatus = get_mali_dvfs_staus();
	nextStatus = decideNextStatus(utilization);

	MALI_DEBUG_PRINT(1, ("= curStatus %d, nextStatus %d, maliDvfsStatus.currentStep %d \n", curStatus, nextStatus, maliDvfsStatus.currentStep));

	/*if next status is same with current status, don't change anything*/
	if((curStatus!=nextStatus && stay_count==0))
	{
		/*check if boost up or not*/
		if(nextStatus > maliDvfsStatus.currentStep) boostup = 1;

		/*change mali dvfs status*/
		if(!change_mali_dvfs_staus(nextStatus,boostup))
		{
			MALI_DEBUG_PRINT(1, ("error on change_mali_dvfs_staus \n"));
			return MALI_FALSE;
		}
		stay_count = mali_dvfs_staycount[maliDvfsStatus.currentStep].staycount;
	}
	else
	{
		if( stay_count>0 )
			stay_count--;
	}
#endif
    return MALI_TRUE;
}



int mali_dvfs_is_running(void)
{
	return bMaliDvfsRun;
}



void mali_dvfs_late_resume(void)
{
	// set the init clock as low when resume
	set_mali_dvfs_staus(0,0);
}


static void mali_dvfs_work_handler(struct work_struct *w)
{
    bMaliDvfsRun=1;

    MALI_DEBUG_PRINT(3, ("=== mali_dvfs_work_handler\n"));

    if(!mali_dvfs_staus(mali_dvfs_utilization))
        MALI_DEBUG_PRINT(1,( "error on mali dvfs status in mali_dvfs_work_handler"));

    bMaliDvfsRun=0;
}


mali_bool init_mali_dvfs_staus(int step)
{
    /*default status
    add here with the right function to get initilization value.
    */
    if (!mali_dvfs_wq)
        mali_dvfs_wq = create_singlethread_workqueue("mali_dvfs");

    /*add a error handling here*/
    maliDvfsStatus.currentStep = step;
    return MALI_TRUE;
}

void deinit_mali_dvfs_staus(void)
{
    if (mali_dvfs_wq)
        destroy_workqueue(mali_dvfs_wq);
    mali_dvfs_wq = NULL;
}

mali_bool mali_dvfs_handler(u32 utilization)
{
    mali_dvfs_utilization = utilization;
    queue_work_on(0, mali_dvfs_wq,&mali_dvfs_work);

    /*add error handle here*/
    return MALI_TRUE;
}

void mali_default_step_set(int step, mali_bool boostup)
{
    mali_clk_set_rate(mali_dvfs[step].clock, mali_dvfs[step].freq);

    if (maliDvfsStatus.currentStep == 1)
	set_mali_dvfs_staus(step, boostup);
}
