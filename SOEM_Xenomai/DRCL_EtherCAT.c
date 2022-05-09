#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <alchemy/task.h>
#include <alchemy/timer.h>

#include "ethercat.h"

#define CLOCK_RES 1e-9 
#define EC_TIMEOUTMON 500
#define LOOP_PERIOD 1e7

RT_TASK loop_task;

char IOmap[2048];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
boolean inOP;
uint8 currentgroup = 0;

void ecat_rt_loop_task(void *none)
{
    RT_TASK *curtask;
    RT_TASK_INFO curtaskinfo;
    RTIME tstart, now;
    int iret = 0;
    int i, j, oloop, iloop, chk;
    needlf = FALSE;
    inOP = FALSE;
    
    curtask = rt_task_self();
    rt_task_inquire(curtask, &curtaskinfo);

  //Print the info
    printf("Starting task %s with period of 10 ms ....\n", curtaskinfo.name);

  //Make the task periodic with a specified loop period
    rt_task_set_periodic(NULL, TM_NOW, LOOP_PERIOD);

    printf("Starting Realtime simple test\n");

   /* initialise SOEM, bind socket to ifname */
if (ec_init("rteth0"))
{
    printf("ec_init on %s succeeded.\n","rteth0");
    /* find and auto-config slaves */


    if ( ec_config_init(FALSE) > 0 )
    {
        printf("%d slaves found and configured.\n",ec_slavecount);

        ec_config_map(&IOmap);

        ec_configdc();

        printf("Slaves mapped, state to SAFE_OP.\n");
        /* wait for all slaves to reach SAFE_OP state */
        ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4);

        oloop = ec_slave[0].Obytes;
        if ((oloop == 0) && (ec_slave[0].Obits > 0)) oloop = 1;
        if (oloop > 8) oloop = 8;
        iloop = ec_slave[0].Ibytes;
        if ((iloop == 0) && (ec_slave[0].Ibits > 0)) iloop = 1;
        if (iloop > 8) iloop = 8;

        printf("segments : %d : %d %d %d %d\n",ec_group[0].nsegments ,ec_group[0].IOsegment[0],ec_group[0].IOsegment[1],ec_group[0].IOsegment[2],ec_group[0].IOsegment[3]);

        printf("Request operational state for all slaves\n");
        expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
        printf("Calculated workcounter %d\n", expectedWKC);
        ec_slave[0].state = EC_STATE_OPERATIONAL;
        /* send one valid process data to make outputs in slaves happy*/
        ec_send_processdata();
        ec_receive_processdata(EC_TIMEOUTRET);
        /* request OP state for all slaves */
        ec_writestate(0);
        chk = 200;
        /* wait for all slaves to reach OP state */
        do
        {
        ec_send_processdata();
        ec_receive_processdata(EC_TIMEOUTRET);
        ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
        }
        while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
        if (ec_slave[0].state == EC_STATE_OPERATIONAL )
        {
        printf("Operational state reached for all slaves.\n");
        inOP = TRUE;
            /* cyclic loop */
        for(i = 1; i <= 10000; i++)
        {
            ec_send_processdata();
            wkc = ec_receive_processdata(EC_TIMEOUTRET);


                    rt_task_wait_period(NULL);

                }
                inOP = FALSE;
            }
            else
            {
                printf("Not all slaves reached operational state.\n");
                ec_readstate();
                for(i = 1; i<=ec_slavecount ; i++)
                {
                    if(ec_slave[i].state != EC_STATE_OPERATIONAL)
                    {
                        printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                            i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
                    }
                }
            }
            printf("\nRequest init state for all slaves\n");
            ec_slave[0].state = EC_STATE_INIT;
            /* request INIT state for all slaves */
            ec_writestate(0);
        }
        else
        {
            printf("No slaves found!\n");
        }
        printf("End simple test, close socket\n");
        /* stop SOEM, close socket */
        ec_close();
    }
    else
    {
        printf("No socket connection on %s\nExecute as root\n","rteth0");
    }
}

OSAL_THREAD_FUNC ecatcheck( void *ptr )
{
    int slave;
    (void)ptr;                  /* Not used */

    while(1)
    {
        if( inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
        {
            if (needlf)
            {
               needlf = FALSE;
               printf("\n");
            }
            /* one ore more slaves are not responding */
            ec_group[currentgroup].docheckstate = FALSE;
            ec_readstate();
            for (slave = 1; slave <= ec_slavecount; slave++)
            {
               if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
               {
                  ec_group[currentgroup].docheckstate = TRUE;
                  if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
                  {
                     printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                     ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state == EC_STATE_SAFE_OP)
                  {
                     printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                     ec_slave[slave].state = EC_STATE_OPERATIONAL;
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state > EC_STATE_NONE)
                  {
                     if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d reconfigured\n",slave);
                     }
                  }
                  else if(!ec_slave[slave].islost)
                  {
                     /* re-check state */
                     ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                     if (ec_slave[slave].state == EC_STATE_NONE)
                     {
                        ec_slave[slave].islost = TRUE;
                        printf("ERROR : slave %d lost\n",slave);
                     }
                  }
               }
               if (ec_slave[slave].islost)
               {
                  if(ec_slave[slave].state == EC_STATE_NONE)
                  {
                     if (ec_recover_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d recovered\n",slave);
                     }
                  }
                  else
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d found\n",slave);
                  }
               }
            }
            if(!ec_group[currentgroup].docheckstate)
               printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(10000);
    }
}


int main(int argc, char **argv){

    char str[40];
    osal_thread_create(&thread1, 128000, &ecatcheck, (void*) &ctime); //Using Pthread(OSAL) for NonRt task ecatcheck

    //Lock the memory to avoid memory swapping for this program
    mlockall(MCL_CURRENT | MCL_FUTURE);

    //Create the real time task

    sprintf(str, "DRCL_Realtime_ECAT_Master_TEST");
    rt_task_create(&loop_task, str, 0, 50, 0);

    //Since task starts in suspended mode, start task
    rt_task_start(&loop_task, &ecat_rt_loop_task, 0);

    pause();

    return 0;
    }


