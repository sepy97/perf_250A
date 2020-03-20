#define MEMTEST 0	//FREQTEST for testing transactions with different frequency, MEMTEST for testing transactions with different size of allocated memory

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <thread>
#include <atomic>

#include <immintrin.h>
#include <x86intrin.h>
#include <chrono>

#define NUMOFTRANS 10000000
#define MAXTHREADS 10


using namespace std;
using namespace std::chrono;
using namespace std::this_thread;

std::atomic<long long unsigned> tx (0);		//counter of successful transactions
std::atomic<long long unsigned> aborts (0);	//counter of aborts
std::atomic<long long unsigned> conflicts (0);
std::atomic<long long unsigned> retry (0);	//only for Intel
std::atomic<long long unsigned> capacity (0);
std::atomic<long long unsigned> nesting (0);
std::atomic<long long unsigned> userAbort (0);
std::atomic<long long unsigned> debug (0); 	//only for Intel

char* tmp;	//the global variable where we make writes inside of transaction

std::thread thr[MAXTHREADS];
cpu_set_t cpuset;

void test (const int volume, int threadNum, int param)
{
//pinning threads to cores	
	CPU_ZERO (&cpuset);
	CPU_SET (threadNum*2, &cpuset);
        pthread_setaffinity_np (thr[threadNum].native_handle(), sizeof(cpu_set_t), &cpuset);

//setting allocated memory to 16 bytes and sleeping period to 1 ms	
	int memory = 16;
	int residue = 1;
	int iter = 0;

	for (int i = 0; i < volume; i++)
	{
		iter = rand() % memory - 2;
		sleep_until(system_clock::now() + milliseconds(residue));
//		if (threadNum*2 != sched_getcpu()) printf ("Threadnum %d is not equal to sched %d\n", threadNum*2, sched_getcpu());

		unsigned status = _xbegin();
		if (status == _XBEGIN_STARTED)
		{
//transactional region
			tmp[iter]   = threadNum;
			tmp[iter+1] = threadNum + 1;
			tmp[iter+2] = threadNum + 2;
			_xend();
			tx++;
		}
		else
		{
//in case of aborts			
			if (status & _XABORT_CONFLICT)
			{
#ifdef DEBUG
				printf ("Conflict! \n");
#endif
				conflicts++;
			}
			else if (status & _XABORT_RETRY)
			{
#ifdef DEBUG
				printf ("Illegal! \n");
#endif
				retry++;
			}
			else if (status & _XABORT_CAPACITY)
			{
#ifdef DEBUG
				printf ("Capacity! \n");
#endif
				capacity++;
			}
			else if (status & _XABORT_NESTED)
			{
#ifdef DEBUG
				printf ("Nested too deep! \n");
#endif
				nesting++;
			}
			else if (status & _XABORT_EXPLICIT)
			{
#ifdef DEBUG
				printf ("User abort! \n");
#endif
				userAbort++;
			}
                        else if (status & _XABORT_DEBUG)
                        {
#ifdef DEBUG
                                printf ("Debug! \n");
#endif
                                debug++;
                        }
			else 
			{
#ifdef DEBUG
				printf ("Unknown reason :( \n");
#endif
			}
			aborts++;
		}
	}


	return;
}

int main (int argc, char** argv)
{
	tmp = (char*) calloc (16, sizeof (char));
	
	CPU_ZERO (&cpuset);
	for (int i = 0; i < MAXTHREADS; i++)
	{
		thr[i] = std::thread (test, NUMOFTRANS/MAXTHREADS, i, 16);	
	}

	void* ret = NULL;
	for (int i = 0; i < MAXTHREADS; i++)
	{
		thr[i].join();
	}

}
