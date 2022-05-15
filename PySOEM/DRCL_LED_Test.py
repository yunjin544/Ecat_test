
from sqlite3 import Timestamp
import sys
import struct
import time
import threading
import pysoem
from collections import namedtuple

usleep = lambda x: time.sleep(x/1000000.0)

class BasicExample:

    def __init__(self, ifname):
        self._ifname = ifname
        self._pd_thread_stop_event = threading.Event()
        self._ch_thread_stop_event = threading.Event()
        self._actual_wkc = 0
        self._master = pysoem.Master()
        self._master.in_op = False
        self._master.do_check_state = False
        SlaveSet = namedtuple('SlaveSet', 'name product_code config_func')

    def _processdata_thread(self):
        while not self._pd_thread_stop_event.is_set():
            self._master.send_processdata()
            self._actual_wkc = self._master.receive_processdata(10000)
            if not self._actual_wkc == self._master.expected_wkc:
                print('incorrect wkc')
            usleep(10)

    def _pdo_update_loop(self):

        self._master.in_op = True

        output_len = len(self._master.slaves[0].output)

        tmp = bytearray([0 for i in range(output_len)])

        toggle = True


######################################################################################
                                                    #
        Frequency = 1000 # Hz                       #           Goal Frequency
                                                    #
######################################################################################

        Loop_time=1/Frequency
        
        try:
            while 1:
                Timestamp = time.perf_counter()
                if toggle:
                    tmp[0] = 1
                else:
                    tmp[0] = 0
                self._master.slaves[0].output = bytes(tmp)

                toggle ^= True
                dt = time.perf_counter()-Timestamp
                while(dt < Loop_time):
                    dt = time.perf_counter()-Timestamp
                print("dt : {} , Frequency : {} Hz".format(dt,1/dt))

        except KeyboardInterrupt:
            # ctrl-C abort handling
            print('stopped')


    def run(self):

        self._master.open(self._ifname)

        if not self._master.config_init() > 0:
            self._master.close()
            raise BasicExampleError('no slave found')

        
        self._master.config_map()

        if self._master.state_check(pysoem.SAFEOP_STATE, 50000) != pysoem.SAFEOP_STATE:
            self._master.close()
            raise BasicExampleError('not all slaves reached SAFEOP state')

        self._master.state = pysoem.OP_STATE

        check_thread = threading.Thread(target=self._check_thread)
        check_thread.start()
        proc_thread = threading.Thread(target=self._processdata_thread)
        proc_thread.start()
        
        # send one valid process data to make outputs in slaves happy
        self._master.send_processdata()
        self._master.receive_processdata(2000)
        # request OP state for all slaves
        
        self._master.write_state()

        all_slaves_reached_op_state = False
        for i in range(40):
            self._master.state_check(pysoem.OP_STATE, 50000)
            if self._master.state == pysoem.OP_STATE:
                all_slaves_reached_op_state = True
                break

        if all_slaves_reached_op_state:
            self._pdo_update_loop()

        self._pd_thread_stop_event.set()
        self._ch_thread_stop_event.set()
        proc_thread.join()
        check_thread.join()
        self._master.state = pysoem.INIT_STATE
        # request INIT state for all slaves
        self._master.write_state()
        self._master.close()

        if not all_slaves_reached_op_state:
            raise BasicExampleError('not all slaves reached OP state')

    @staticmethod
    def _check_slave(slave, pos):
        if slave.state == (pysoem.SAFEOP_STATE + pysoem.STATE_ERROR):
            print(
                'ERROR : slave {} is in SAFE_OP + ERROR, attempting ack.'.format(pos))
            slave.state = pysoem.SAFEOP_STATE + pysoem.STATE_ACK
            slave.write_state()
        elif slave.state == pysoem.SAFEOP_STATE:
            print(
                'WARNING : slave {} is in SAFE_OP, try change to OPERATIONAL.'.format(pos))
            slave.state = pysoem.OP_STATE
            slave.write_state()
        elif slave.state > pysoem.NONE_STATE:
            if slave.reconfig():
                slave.is_lost = False
                print('MESSAGE : slave {} reconfigured'.format(pos))
        elif not slave.is_lost:
            slave.state_check(pysoem.OP_STATE)
            if slave.state == pysoem.NONE_STATE:
                slave.is_lost = True
                print('ERROR : slave {} lost'.format(pos))
        if slave.is_lost:
            if slave.state == pysoem.NONE_STATE:
                if slave.recover():
                    slave.is_lost = False
                    print(
                        'MESSAGE : slave {} recovered'.format(pos))
            else:
                slave.is_lost = False
                print('MESSAGE : slave {} found'.format(pos))
    
    def _check_thread(self):

        while not self._ch_thread_stop_event.is_set():
            if self._master.in_op and ((self._actual_wkc < self._master.expected_wkc) or self._master.do_check_state):
                self._master.do_check_state = False
                self._master.read_state()
                for i, slave in enumerate(self._master.slaves):
                    if slave.state != pysoem.OP_STATE:
                        self._master.do_check_state = True
                        BasicExample._check_slave(slave, i)
                if not self._master.do_check_state:
                    print('OK : all slaves resumed OPERATIONAL.')
            usleep(10)


class BasicExampleError(Exception):
    def __init__(self, message):
        super(BasicExampleError, self).__init__(message)
        self.message = message


if __name__ == '__main__':

    print('EtherCAT Master Test started')

    BasicExample('enp3s0').run()
