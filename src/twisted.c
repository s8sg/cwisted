/*********************************************************
 * Version: 0.1.0
 *
 * File Info: File holds the definetion of twisted functions
 *            and core logic.
 *********************************************************/ 

#include "../include/twisted.h"   /* Header defines all the necessery definations */

/* Defination of internal function used by twisted */
static st_trefnode *_new_thread_node(void(*fun_ptr)(void*),void *);
static st_twisthread * _get_ref_byid(int);
static int _bring_in_readyq(int);
static st_event* _get_event_ref_byid(int);
static int _readyq_priority_sort(void);
static void _thread_scheduler(void); 
void _RRscheduler_clock(void*);
static void _RR_schedule_handler(int);

/* Defination of Internal variable used by twisted */ 
static struct twisted_core g_twisted_core;
static struct eventreffmap g_event_map;
static struct interruptreffmap g_interrupt_map;
static struct keymap g_key_map;
static int twist_init_ver = 0;    

/* _RRscheduler_clock() : process that works as a clock for 
 *                       Round Robin Scheduling.
 * @data :               NULL. For future use.
 */
void _RRscheduler_clock(void* data) {
   int ppid;
   ppid = getppid();
   printf("new p: In new process: %d\n", ppid);
   while(1) {
       printf("new P:(%d) Sending signal to (%d)\n", getpid(), ppid);
       kill(ppid, twisted_SIGNAL);
       printf("new p:Sleep time: %d\n", g_twisted_core.scheduler.RR_sleep_time);
       sleep(g_twisted_core.scheduler.RR_sleep_time);
   } 
}

static void _RR_schedule_handler(int sig) {

   twisted_info(">> Signal recived: \n");
   if(g_twisted_core.scheduler.inthread == TRUE) {
       twist_yeild();
   }
}
    
/* _thread_scheduler(): function to schedule threads in readyqueue 
 * return:    on success: return 0, 
 *            on failure: returns Error code
 *
 * readyqueue threads are scheduled by this function. it schedules 
 * threads and jumps to the conext of schedule thread 
 */  
static void _thread_scheduler(void) {  
       
     st_trefnode *ref_current = NULL;     /* current thread inode ref */
     st_twisthread *thread = NULL;
     /* check if PRIORITY */ 
     //else if(g_twisted_core.scheduler.type == PRIORITY){
          /* short ready queue in basis of priority */
     printf("Before short: ");
     print_ready_queue_();
     _readyq_priority_sort();
     printf("After Short short: ");
     /*XXX*/print_ready_queue_();
     /* get the ref of the thread to be dispatched */
     ref_current = QUEUE_PEEK(g_twisted_core.ready_queue);
     thread = (st_twisthread *)ref_current->data;
     /* set the state as RUNNING */ 
     thread->state = RUNNING;
     /* set inthread flag */ 
     g_twisted_core.scheduler.inthread = TRUE; 
     /* set curent thread in scheduler */
     g_twisted_core.scheduler.current_thread = thread;		  
     printf("Address of buffer: %p \n", thread->buf);
     longjmp(thread->buf, D_LJ_RETURN);
}

/* twisted_init(): Initialize the core of twisted 
 * return:    on success: return 0, 
 *            on failure: returns Error code
 *
 * Initialize all the internal structures of thread library
 */ 
int twist_init(void) {
 
    int count;  /* initialize counter */

    if(twist_init_ver != 0) {
       twisted_error("twisted core is already initialized");
       return -E_ALRDYINIT;
    }

    /* init thread reff map structure */
    twisted_info("\ninit THREAD REFF MAP... \n");
    g_twisted_core.thread_ref_map.array = NULL;
    g_twisted_core.thread_ref_map.nos = 0;
    /* init readyqueue structure */
    twisted_info("init READYQUEUE... \n");
    g_twisted_core.ready_queue = create_queue();
	    
    /* init scheduler structure */
    twisted_info("init SCHEDULER... \n");
    g_twisted_core.scheduler.current_thread = NULL;
    g_twisted_core.scheduler.inthread = FALSE;
    g_twisted_core.scheduler.ininterrupt = 0;
    g_twisted_core.scheduler.type = PRIORITY;
    g_twisted_core.scheduler.RR_sleep_time = D_RR_TIME;
    g_twisted_core.scheduler.clock_pid = 0;
    g_twisted_core.scheduler.clock_pstack = NULL;
    
    /* init event reff map structure */
    twisted_info("init EVENTS REFF MAP... \n");
    g_event_map.list = NULL;
    g_event_map.nos = 0;
    /* init interrupt reff map */
    twisted_info("init st_interrupt REFF MAP... \n");
    while(count < MAX_INTERRUPT){
        g_interrupt_map.array[count].id = count + 1;
        g_interrupt_map.array[count].list = NULL;
        g_interrupt_map.array[count].nos = 0;
        count++;    
    }
    /* init key reff map structure */
    twisted_info("init KEY REFF MAP... \n");
    g_key_map.array = NULL;
    g_key_map.nos = 0;        
    /* set global init ver */
    twist_init_ver = TRUE;
    
    return SUCCESS;
}

/* _create_signal_handler(): signal handler body for new thread 
 * sig :   send signal number
 *
 * internal function for signal handler which will be called to 
 * new signal stack while creating thread 
 */ 
static void _create_signal_handler(int sig) {

    st_twisthread *ref_current = NULL;   /* stores the current_thread ref */

    ref_current = g_twisted_core.scheduler.current_thread;
    printf("Current thread: %p \n", ref_current);
    /* check is current ref in NULL */
    if(ref_current == NULL) {
        twisted_error("Current is NULL (Thread creation failed)");
        return;
    }
    /* save context of the newborn thread 
     * this code only executes at creation of a thread */
    if(setjmp(ref_current->buf)) {
        g_twisted_core.scheduler.inthread = TRUE;
        /* call the thread body and pass the args */        
        ref_current->fptr(ref_current->args);
        ref_current->exit_status = SUCCESS;            
        ref_current->state = TERMINATE;
        /* check whether there exists at two threads in readyqueue
         * if threre call thread scheduler */    
        if( QUEUE_SIZE(g_twisted_core.ready_queue) > 1) {
            /* advance readyqueue and unlink current_thread*/
	    dqueue(g_twisted_core.ready_queue);

//            g_twisted_core.ready_queue.start = g_twisted_core.ready_queue.start->next;
  //          g_twisted_core.ready_queue.start->prev = NULL;
            /* decrement the thread count in ready_queue */
    //        g_twisted_core.ready_queue.nos--;
            /* call thread scheduler */
            _thread_scheduler();                
        }
        /* if no thread in ready queue go for mainthread last saved 
         * context */
        else {
            /* set cuurent_thread as NULL */
            g_twisted_core.scheduler.current_thread = NULL;
            /* set inthread as FALSE */
            g_twisted_core.scheduler.inthread = FALSE;       
            /* unlink current_thread from ready_queue */ 
	    dqueue(g_twisted_core.ready_queue);
            
	    //g_twisted_core.ready_queue.nos = 0;
            //g_twisted_core.ready_queue.start = NULL;
            //g_twisted_core.ready_queue.end = NULL;
            //ref_current->next = NULL;
            //ref_current->prev = NULL;
            /* jump to the last saved main context */ 
            longjmp(g_twisted_core.scheduler.main, D_LJ_RETURN);        
        }      
        /* NOTE: we can't free the stack memory in the sig_handler
         *       as the sig_handler had been called to the allocted 
         *       stack. It would be freed when the thread is freed 
         * */      
    }
    else {
        twisted_info("Thread succesfully created.. \n");
    }
}

/* _new_thread_node(): create new thread node 
 * t_funptr : pointer to the function to be used as thread body
 * t_args:    arguments to be passed to the thread
 * return:    on success: returns the thread ref node reference, 
 *            on failure: returns NULL
 *
 * internal function allocates and initializes thread map node 
 * and thread node
 */ 
static st_trefnode *_new_thread_node(void(*fun_ptr)(void*), void *args) {

    st_twisthread *newthread = NULL;    /* ref of newnode */
    st_trefnode *newnode = NULL;    /* ref of newnode */

    
    /* allocate memory for newnode */
    newthread = (st_twisthread*)twisted_malloc(sizeof(st_twisthread));
    if(newthread == NULL) {
         twisted_free(newnode);    
         return NULL; 
    }
    /* initialize thread with defatwist parameter */
    newthread->fptr = fun_ptr;
    newthread->args = args;
    /* update thread reference count */
    newthread->id = ++(g_twisted_core.thread_ref_map.nos);
    newthread->priority = D_PRIORITY;
    newthread->u_priority = D_PRIORITY;
    newthread->lock_owned = INVALID;
    newthread->exit_status = -D_EXITSTATUS;        
    newthread->state = NEW;

    /* allocate memory for ref node */
    newnode = create_node(newthread);

    /* return new node */
    return newnode;
}

/* twist_create(): create user level thread
 * t_funptr : pointer to the function to be used as thread body
 * t_args:    arguments to be passed to the thread
 * return:    on success: returns the thread id, 
 *            on failure: returns Error Code
 *
 * creates user level thread and returns the thread id.
 * created thread need to be explicitly started by user.
 */      
int twist_create(void(*t_funptr)(void*), void *t_args) {    

    struct sigaction handler;       /* new sig_handler to be set for thread (used to set signal stack) */
    struct sigaction oldHandler;    /* user sig_handler need to be restored for current signal stack */ 
    stack_t oldStack;               /* holds the user signal stack */
    st_trefnode * temp_node = NULL; /* to hold temp_node from thread ref arrary */  
    st_trefnode * new_node = NULL;  /* to hold the newly created thread */
    st_twisthread * thread = NULL;
    int ret = FAILURE;
   
    temp_node = g_twisted_core.thread_ref_map.array;
    new_node = _new_thread_node(t_funptr, t_args);
    /* Check is node creation fails */
    if(new_node == NULL) {
        twisted_error("Memory allocation failed for node");
        return -E_MEMALLOC;
    }
 
    /* set current as new_node */   
    g_twisted_core.scheduler.current_thread = (st_twisthread*) new_node->data;
   
    thread = (st_twisthread*) new_node->data; 

    /* create the new signal stack */
    thread->stack.ss_flags = SS_ONSTACK;

    /* check arcitecture for stack size */
    if(sizeof(size_t) > 4){ 
        thread->stack.ss_size = ULT_STACK_SIZE_64BIT;
        twisted_info("64 bit arcitecture, stack size: %d bytes \n", ULT_STACK_SIZE_64BIT);
    }
    else if(sizeof(size_t) > 2){
        thread->stack.ss_size = ULT_STACK_SIZE_32BIT;
        twisted_info("64 bit arcitecture, stack size: %d bytes \n", ULT_STACK_SIZE_32BIT);
    }
    else{
        thread->stack.ss_size = ULT_STACK_SIZE_16BIT;
        twisted_info("64 bit arcitecture, stack size: %d bytes \n", ULT_STACK_SIZE_16BIT);
    }    
    /* Allocate memory for stack */
    //thread->stack.ss_sp = twisted_malloc(new_node->thread->stack.ss_size);
    thread->stack.ss_sp = twisted_malloc(1024*1024);
    thread->stack.ss_size = 1024*1024; 
    
    if(thread->stack.ss_sp == NULL) { 
        twisted_error("Memory allocation failed for stack");
        twisted_free(thread);
        twisted_free(new_node);
        return -E_MEMALLOC;
    }
    /* fill stack with 1's */
   // memset(new_node->thread->stack.ss_sp, 1, new_node->thread->stack.ss_size);         
    /* Install the new stack for the signal handler 
     * stack direction is autometically managed..( ha ha ki moja !!! ) */
    if((ret = sigaltstack(&thread->stack, &oldStack)) != 0) {
        twisted_error("signal stack alteration failed");
        twisted_free(thread);
        twisted_free(new_node);
        return -E_STACKCREATE;     
    } 
  
    /* Install the signal handler */
    /* Sigaction --must-- be used so we can specify SA_ONSTACK */
    handler.sa_handler = &_create_signal_handler;
    /* set flags:   SA_ONSTACK -> to call on specified stack
     *              SA_NODEFER -> to make SIGUSR1 available inside thread
     */
    handler.sa_flags = SA_ONSTACK | SA_NODEFER;
    /* empty mask set */
    sigemptyset(&handler.sa_mask);
    /* set sig action */ 
    if((ret = sigaction(twisted_SIGNAL, &handler, &oldHandler)) != 0) {
        twisted_error("signal action alteration failed");
        twisted_free(thread);
        twisted_free(new_node);
        return -E_STACKCREATE;     
    } 
        
    /* Raise signal to call the handler on the new stack */ 
    raise(twisted_SIGNAL);    
        
    /* Restore old stack and handler for user ( ata kora khub dorkar ) */
    if((ret = sigaltstack(&oldStack, NULL)) != 0) { 
        twisted_error("signal stack restore failed");
        twisted_free(thread);
        twisted_free(new_node);
        return -E_STACKCREATE;     
    }
    /* restore old signal handler for user */
    if((ret = sigaction(twisted_SIGNAL, &oldHandler, 0)) != 0) {
        twisted_error("signal action alteration failed");
        twisted_free(thread);
        twisted_free(new_node);
        return -E_STACKCREATE;    
    }

    /* Set current as NULL */
    g_twisted_core.scheduler.current_thread = NULL;        

    /* check if array is empty */
    if(temp_node!=NULL) {    
        /* traverse until end */
        while(NOT_LAST(temp_node)) {
           temp_node = temp_node->next;
        }
        /* add new_node at the end */
        temp_node->next = new_node;       
    }
    else {
        /* add first element to reference array */
        g_twisted_core.thread_ref_map.array = new_node;
    }

    /* return new thread id */       
    printf("Thread ref :: %p \n", thread);
    return thread->id;
}


/* twist_exit():  Terminates current thread and saves return status 
 * status:      exit status of the thread
 * return:      On success returns 0
                else returns error code
 * it terminates the thread from which it is called and return the status
 * given by user 
 */
int twist_exit(int status) {

    int ret = SUCCESS;                     /* the return value */
    st_twisthread * thread = NULL;      /* holds current thread  reference */
  
    thread = g_twisted_core.scheduler.current_thread;
    /* check if in thread as it should be called from a thread */ 
    if(g_twisted_core.scheduler.inthread == TRUE) {

        /* update exit status */
        thread->exit_status = status;
        /* set state as ABORT */
	thread->state = ABORT;
        
	/* update ready queue */
	dqueue(g_twisted_core.ready_queue);

        //g_twisted_core.ready_queue.start = g_twisted_core.ready_queue.start -> next;
        //if(g_twisted_core.ready_queue.start != NULL) {    
        //    g_twisted_core.ready_queue.start -> prev = NULL;
        //}
        //g_twisted_core.ready_queue.nos--;   

        /* call scheduler if ready queue has more thread */ 
        if(QUEUE_SIZE(g_twisted_core.ready_queue) > 0) {
            _thread_scheduler();
        }
        /* no thrdead in readyqueue so execute main */
        else {            
            /* set current as NULL */
            g_twisted_core.scheduler.current_thread = NULL;   
            g_twisted_core.scheduler.inthread = 0;


	    /* update ready queue */
	    dqueue(g_twisted_core.ready_queue);
            /* update ready queue */   
            //g_twisted_core.ready_queue.nos = 0;
            //g_twisted_core.ready_queue.start = NULL;
            //g_twisted_core.ready_queue.end = NULL;
            /* jump to last saved main context */
            longjmp(g_twisted_core.scheduler.main, D_LJ_RETURN);        
        }
    }    
    else {
        twisted_error("\n\t Should be called from a thread !\n");
        /* set return as FAILURE */
        ret =  FAILURE;
    }        
    /* return status (This function can only return on FAILURE) */
    return ret;
}


/* _get_event_ref_byid():  get event ref by passing event id
 * @event_id :   id of the event
 * @return :     on success returns event ref
 *               on failure retunrs null
 */
static st_event* _get_event_ref_byid(int event_id) {
    
    st_eventnode *temp_event = NULL;          /* temp event node ref */  
    st_event *event = NULL;          /* temp event ref */  

    /* set event ref array */
    temp_event = g_event_map.list; 
    if(temp_event == NULL) {
        return NULL;
    }
    /* traverse till end on event ref array 
     * until the event ref got found */      
    while(temp_event != NULL){
	event = (st_event*)temp_event->data;
        if(event->id == event_id) {
            break;        
        }
        temp_event = temp_event->next;    
    }
    /* return status */
    return event;
}


/* twist_get_stackspace_() : get free space in specified thread stack
 * @thread_id :  id of the thread
 * @return :     on success return the aprox stack space remains
 *               in the thread stack
 *               on failure returns -1
 *
 * Used for debugging purpose. Returns only unused(untouched) space
 * in stack. 
 */
int twist_get_stackspace_(int thread_id){
   
   int byte_counter = FAILURE;            /* counter for free byte */
   st_twisthread *thread_ref = NULL;   /* ref of the threadc */

   /* get the reference of the thread */
   thread_ref = _get_ref_byid(thread_id);
   if(thread_ref != NULL){
       byte_counter = 0;
       while(byte_counter < thread_ref->stack.ss_size){
           /* check if any bit is changed due to call in stack then break */
           if(*((char*)(thread_ref->stack.ss_sp + byte_counter)) != 1){
               break;
           }
           /* increment byte counter */
           byte_counter++;
       }
   }
   return byte_counter;
}


/* print_waiting_threads_() : prints the thread's ID waiting for the event which
 *                            is specified by the ID
 * @event_id : ID of the event
 * @return   : On success returns the no of thread is waiting
 *             On failure returns -1
 *
 * Mainly used for Debugging purpose. Prints the thread in the waiting state for
 * the event specified by the event ID
 */ 
int print_waiting_threads_(int event_id) {

    st_event * event_ref = NULL;               /* reference of the event */
    st_trefnode * temp_thread_ref = NULL;      /* temp reference of thread ref node */
    st_twisthread *thread = NULL;
    int ret_val = FAILURE;

    /* get the reference of the event by it's ID */        
    event_ref = _get_event_ref_byid(event_id);
    /* check if event ID is valid */
    if(event_ref != NULL) {
    
        temp_thread_ref = event_ref->array;
        /* prints the waiting thread */
        while(temp_thread_ref != NULL) {
	    thread = (st_twisthread*) temp_thread_ref->data;
            twisted_info(" (%d | %d) ->",thread->id,thread->priority);    
            temp_thread_ref = temp_thread_ref->next;            
        }
        twisted_info(" (NULL)\n");
        ret_val =  event_ref->nos;        
    }    
    else {
        twisted_error("wrong event id");                    
    }
    /* return value */
    return ret_val;
}


/* twist_create_event(): creates an event for which threads will be waiting
 * @return :    on success returns event id
 *              on failure returns -1
 * 
 * thread can be sent to waiting state until a event occurs. For a single
 * event mtwistiple thread can wait. Event is recognised by the event id
 * returned.
 */
int twist_getevent(void){

    st_event *new_event = NULL;     /* new event node ref */
    st_eventnode *new_event_node = NULL;    /* temp ref for event */
    st_eventnode *temp_event = NULL;    /* temp ref for event */
    
    /* allocate memory for event */ 
    new_event = (st_event*)malloc(sizeof(st_event));
    /* check if new event is NULL */
    if(new_event == NULL) {    
        twisted_error("malloc error");
        return FAILURE;
    }

    /* initialize event */
    new_event->nos = 0;    
    /* get event id */
    new_event->id = ++g_event_map.nos;
    new_event->array = NULL;

    new_event_node = create_node(new_event);

    /* add event to eventy map */ 
    /* check if it is the first element in array */
    if(g_event_map.list == NULL) {
        g_event_map.list = new_event_node;
    }
    /* add new elrment in the end of list */
    else {
	temp_event = g_event_map.list;
        while(temp_event->next != NULL) {
            temp_event = temp_event->next;    
        }
        temp_event->next = temp_event;    
    }

    /* return event id */ 
    return new_event->id;
}


/* twist_wait() : send current thread to waiting state for a specified event
 * @event_id  : the ID of the event for which the therad will be waiting
 * @return    : on success return 0
 *              on failure return -1
 * 
 * Thread to which it's called will be in waiting thread twistill the 
 * waiting event being notified 
 */
int twist_wait(int event_id){

    st_event * event_ref = NULL;     /* reference of the specified event */
    st_trefnode * temp_ref = NULL;   /* temp reference to waiting thread */

    /* get event ref by specified event_id */
    event_ref = _get_event_ref_byid(event_id); 
    /* check if event id is valid */
    if(event_ref == NULL) {
        twisted_error("Invalid event id \n");
    }   
    /* get ref of the first thread */ 
    temp_ref = event_ref->array;

    /* check pre conditions */
    if(!g_twisted_core.scheduler.inthread){
        twisted_error("Should be called from a thread \n");
        return FAILURE; 
    } 
    if(g_twisted_core.scheduler.ininterrupt){
        twisted_error("Should not be called from a interrupt handler\n");
        return FAILURE; 
    }
   
    /* save context to of cuurent thread going to waiting state */
    if(!setjmp(g_twisted_core.scheduler.current_thread->buf)){            
        /* increment nos of waiting thread */ 
        event_ref->nos++;
        /* check if first thread to add */
        if(event_ref->array == NULL){
            event_ref->array = (st_trefnode*)malloc(sizeof(st_trefnode));
            event_ref->array->data = create_node(g_twisted_core.scheduler.current_thread);
            event_ref->array->next = NULL;
        }
        else{
            while(temp_ref->next != NULL){
                temp_ref= temp_ref->next;
            }        
            temp_ref->next = (st_trefnode*)malloc(sizeof(st_trefnode));
            temp_ref->next->data = create_node(g_twisted_core.scheduler.current_thread);
            temp_ref->next->next = NULL;    
        }    

        /* set core values */
        g_twisted_core.scheduler.current_thread->state = WAITING;

	/****** TODO: Queue Handling*******/

        /* remove current thread from ready queue */
	dqueue(g_twisted_core.ready_queue);
        //g_twisted_core.ready_queue.start = g_twisted_core.ready_queue.start->next;
        //if(g_twisted_core.ready_queue.start != NULL)                
         //   g_twisted_core.ready_queue.start->prev = NULL;
        //g_twisted_core.ready_queue.nos--;

        /* go for scheduling new thread or jump to main */
        if(QUEUE_SIZE(g_twisted_core.ready_queue) > 0) {    
            /* call scheduler to schedule new thread */
            _thread_scheduler();
        }
        else {
	    dqueue(g_twisted_core.ready_queue);
            g_twisted_core.scheduler.current_thread = NULL;
            g_twisted_core.scheduler.inthread = 0;        
            //g_twisted_core.ready_queue.nos = 0;
            //g_twisted_core.ready_queue.start = NULL;
            //g_twisted_core.ready_queue.end = NULL;
            /* jump to main context */
            longjmp(g_twisted_core.scheduler.main, D_LJ_RETURN);        
        }
    }
    /* return success after successfully coming back from wait state */
    return SUCCESS;
}


/* twist_notify() : notify threads waiting for a event to bring 
 *                in ready state
 * @event_id    : ID of the event to notify the threads
 * @return      : on success return no of thread was waiting
 *                on failure return -1
 * 
 * brings thread to ready state from waiting state, waiting for 
 * event specified by event id
 */
int twist_notify(int event_id) {
    
    st_event * event_ref = NULL;
    st_trefnode * temp_thread_ref = NULL;
    st_twisthread * thread = NULL;
    int thread_nos = 0;

    /* get reference of the event */
    event_ref = _get_event_ref_byid(event_id);
    if(event_ref == NULL){
        twisted_error("Invalid event id");    
        
    }
    /* get thread ref array */
    temp_thread_ref = event_ref->array; 
    /* get no of thread waiting */
    thread_nos = event_ref->nos;
    /* if no thread is waiting */
    if(thread_nos == 0){
	/* return with current thread nos */
        return thread_nos;
    }

    thread = (st_twisthread *)temp_thread_ref->data;
    /* bring threads to ready queue */ 
    while(temp_thread_ref != NULL){
        _bring_in_readyq(thread->id);
        temp_thread_ref = temp_thread_ref->next;
    }
    /* reset event variables */
    event_ref->nos = 0;
    event_ref->array = NULL;
    /* check if called from thread */
    if(!g_twisted_core.scheduler.inthread){
         /* save main context and schedule threads */
        if(!setjmp(g_twisted_core.scheduler.main)){
             _thread_scheduler();
        }
        else{
             g_twisted_core.scheduler.inthread = 0;
        }
    }
    /* return no of thread was waiting for the event */    
    return thread_nos;    
}


/* _get_ref_byid() : to get address of the thread of specified thread id
 * @thread_id :       id of the thread
 * @return    :       on success returns the reference of the thread
 *                    on failure returns the NULL
 * 
 * internal interface to get reference by the thread id
 */
static st_twisthread *_get_ref_byid(int thread_id){
   
    st_trefnode *node = NULL;        /* temp variable to iterate on reference map */
    st_twisthread *thread_ref = NULL;  /* variable to store ref node */

    /* get the start of ref array */
    node = g_twisted_core.thread_ref_map.array;
    /* traverse till the end untill the tid can be found */
    while(node != NULL) {
	thread_ref = (st_twisthread*) node->data;
        /* check if thread id same */
        if(thread_ref->id == thread_id) {
                break;
        }
        /* advance temp reference */
        node = NODE_NEXT(node);
    }
    /* return thread reference */
    return thread_ref;
}


/* _bring_in_readyq(): it puts the thread in readyqueue
 * @thread_id : Id of the thread
 * @return    : on success return 0
 *              else returns error code
 * 
 * Internal function used to bring (link) a thread in the readyqueue
 * thead id is passed, tells which thread to bring in.
 */
static int _bring_in_readyq(int thread_id) {

    st_twisthread * thread_ref = NULL;       /* ref of thread */
    st_trefnode *thread_node = NULL; 
    
    
    /* get the reference of the thread by id */
    thread_ref = _get_ref_byid(thread_id);
    /* check if thread is invalid */
    if(thread_ref == NULL) {
        twisted_error("\n\tNo thread reff found for tid\n");
        return -E_INVALID;
    }
    /* set thread state as READY as it is ready to be dispatched
     * by scheduler */   
    thread_ref->state = READY;

    thread_node = create_node(thread_ref);
    enqueue(g_twisted_core.ready_queue, thread_node);
    
    /* check if no item in readyqueue */
    //if(g_twisted_core.ready_queue.start == NULL) {
    //     g_twisted_core.ready_queue.start = thread_ref;
    //     g_twisted_core.ready_queue.end = thread_ref;
   // }
    /* add ready queue to the tail of the readyqueue list */
    //else {
    //    g_twisted_core.ready_queue.end->next = thread_ref;
    //    thread_ref->next = NULL;
    //    thread_ref->prev = g_twisted_core.ready_queue.end;
    //    g_twisted_core.ready_queue.end = thread_ref;
    //}
    /* increment the nos of element in readyqueue */
    //g_twisted_core.ready_queue.nos++; 

    /************************************/

    return SUCCESS;
}


/* _readyq_priority_sort(): sort the readyqueue in a FCFS priority basis. 
 *                          Thread with same priority is scheduled in a FCFS manner.
 *                          A special check that prevent the start data insertion in case 
 *                          it is called from main 
 */
// The awesome Code written By Mugdha
//
// TODO: Improvise
static int _readyq_priority_sort(void) {        
   return 0;
}

/* twist_start(): It starts execution 'nos' of thread from 'thread_array'
 * thread_array: array holding threads ids
 * nos: nos of thread to be started  
 * return: on success return 0
 *         else returns error code
 * 
 * brings nos of thread in readyqueue, and schedules for execution 
 */
int twist_start(int *thread_array, int nos) {

    int counter = 0;                 /* counter for iteration over thread_array */    
    int new_count = 0;               /* counts the no of new thread */
    st_twisthread *thread_ref = NULL;  /* temp ver to store thread_ref */
    int ret = SUCCESS;               /* return value */
    int t_id = INVALID;              /* temp thread id */ 

    /* check if thread_array is NULL or Nos of thread is zero */
    if(thread_array == NULL || nos <= 0) {
         twisted_error("Invalid args passed by user");
         return -E_INVALPARAM;
    }
    /* iterate over thread_array and check whether that can be scheduled */
    for(counter = 0; counter < nos; counter++) {        
        t_id = thread_array[counter];
        /* get the thread ref by thread id */
        thread_ref = _get_ref_byid(t_id);
        printf("Thread ref: %p \n", thread_ref);
        /* check if thread reference not found or invalid thread id*/
        if(thread_ref == NULL) {
            twisted_error("Invalid thread id: %d at pos: %d", t_id, counter);
            ret = -E_INVALID;
            continue;
        }
        /* check if thread is already started */
        if(thread_ref->state != NEW) {    
            twisted_info("Thread of id: %d has already started",t_id);
            continue;
        }
        /* bring the thread in readyqueue for scheduling */
        ret = _bring_in_readyq(t_id);
        if(ret < 0) {
            twisted_error("Failed to start thread of id: %d - ERROR_CODE: %d", t_id, ret);
            continue;
        }
        /* increment new thread count */     
        new_count++;      
    }
    twisted_info("new Thread count: %d", new_count);
    /* XXX */ /*printreadyqueue();*/
    /* check if thread is sporned inside a scheduler handler 
     * In that case it will not schedule out current thread
     * only makes new threads ready for execution and return
     */
    if(g_twisted_core.scheduler.ininterrupt == TRUE) { 
        /* return status */       
        return ret;   
    }  
    /* check if any new thread to start*/
    if(new_count <= 0) {
        /* return status */    
        return ret;
    }
    twisted_info("thread starts from (0: main and 1: thread): %d\n", g_twisted_core.scheduler.inthread);
    /* check if code started from main */
    if(g_twisted_core.scheduler.inthread == FALSE) {
        /* save main context and call thread scheduler */
        if(!setjmp(g_twisted_core.scheduler.main)) {
            _thread_scheduler();
        }
        else {
             /* return by longjump */
        }
    } 
    /* thread is started from another thread */       
    else {
        /* save current thread context and call scheduler */
        if(!setjmp(g_twisted_core.scheduler.current_thread->buf)) {
		
             _thread_scheduler();
        }
        else {       
             /* return by longjump */
        }  
    }
    /* return status */
    return ret;
}

/* set_twist_priority() : used to set priority of a thread 
 * thread_id      : id of the thread 
 * priority       : the priority of the thread (1 - 10)
 * return         : on success returns 0
 *                  on failure returns error code
 * priority of a thread specified by thread_id can be 
 * changed at runtime by set_twist_priority. It can be called   
 * from inside or outside of the thread 
 * */
int set_twist_priority(int thread_id, int priority) {
    
    st_twisthread *thread = NULL;     /* ref of the thread of thread_id */

    /* check if priority value is valid */
    if(priority > 10 && priority < 1) {    
        twisted_error("Wrong priority value");
	return -E_INVALPRIORITY;
    }
    /* get reference of the thread */
    thread = _get_ref_byid(thread_id); 
    /* check if thread_id is valid */ 
    if(thread == NULL) {
        twisted_error("Wrong thread id");
        return -E_INVALID;
    }
    /* set priority value */
    thread->u_priority = priority;
    thread->priority = priority;
    /* return status */
    return SUCCESS;
}


/* twist_get_priority() : used to get priority of a thread 
 * thread_id      : id of the thread 
 * return         : on success returns thread priority
 *                  on failure returns -1
 * get priority of a specified thread by thread id. It can be called   
 * from inside or outside of the thread 
 * */
int twist_get_priority(int thread_id) {

    st_twisthread *thread = NULL;       /* reference of the thread */
    int ret = FAILURE;                /* ret value */

    /* get the reference of the thread */
    thread = _get_ref_byid(thread_id);
    if(thread != NULL) {
        ret = thread->u_priority;
    }
    /* ref not found */
    else {
        twisted_error("Wrong thread id");    
        ret = INVALID;    
    }
    /* return status */
    return ret;
}


/* twist_get_state_() : Get state of the thread specified by thread Id 
 * @thread_id    :  Id of the thread
 * @return       :  On success state of the thread 
 *                     (NEW, READY, RUNNING, WAITING,TERMINATE, ABORT)
 *                  On failure return -1    
 * 
 * Used for debugging purpose. Get the state of the thread which is 
 * still not freed 
 * */
int twist_get_state(int thread_id) {

    st_twisthread * thread = NULL;      /* ref of the thread */
    int ret = INVALID;                /* ret value for state */

    /* get the reference of the thread by Id */
    thread = _get_ref_byid(thread_id);
    /* check if thread is valid */
    if(thread != NULL) {
        /* set ret as thread current state */
        ret = thread->state;
    }
    else {
        twisted_error("Wrong thread id");    
        ret = INVALID;    
    }
    /* return status */
    return ret;
}


/* twist_get_tid() : get current active thread id
 * return    :    on success return current thread id
 *                on failure return -1
 * returns the current running thread id. Should be called
 * within the thread which id is required
 * */
int twist_get_tid() {
    
    st_twisthread *ref_current = NULL;   /* reference of the thread */
    int ret = INVALID;         /* return value */
    
    /* getting the reference of the current thread */
    ref_current = g_twisted_core.scheduler.current_thread;
    /* check if called from inside of a thread and current reference
     * is not NULL */  
    if(g_twisted_core.scheduler.inthread && ref_current != NULL) {
	/* set return as current thread id */    
        ret = ref_current->id;
    }    
    else {
        twisted_error("Should be called from a thread\n");
    }
    /* return status */
    return ret;
}


/* twist_yeild(): function to schedule out current thread
 * 
 * function used to schedule out current thread and dispatch new 
 * thread after rescheduling. Should be called within a thread 
 */
void twist_yeild(void) {

    st_twisthread *ref_current = NULL;    /* current thread reference */	
    
    /* check if called from inside of thread */
    if(!g_twisted_core.scheduler.inthread) {
        twisted_error("Should be called from a thread");
	return;
    }
    /* check if called inside of an interrupt */
    if(g_twisted_core.scheduler.ininterrupt) {
        twisted_error("should not be called inside an interrupt");
        return;
    }
    /* get current thread reference */
    ref_current = g_twisted_core.scheduler.current_thread;
    /* set current thread state as READY */
    printf("Ref current: %p r nos: %d\n", ref_current, QUEUE_SIZE(g_twisted_core.ready_queue));
    ref_current->state = READY;
    /* save current context of the thread and set jump */ 
    if(!setjmp(ref_current->buf)) {
        /* check if readyqueue has more than one thread 
	 * call scheduler to schedule threads in ready queue */
        if(QUEUE_SIZE(g_twisted_core.ready_queue) > 1) {
            printf("Going to schedule thread \n");
            _thread_scheduler();
        }
	/* jump to last saved main context if no thread in readyqueue */
        else {
             /* set inthread as 0 */
             g_twisted_core.scheduler.inthread = FALSE;
	     /* set current thread ref as NULL */
             g_twisted_core.scheduler.current_thread = NULL;
	     /* jump to last saved main cotext */
             longjmp(g_twisted_core.scheduler.main, D_LJ_RETURN);
        }              
    }
    else {
            /* return to previously saved thread environment */
    }
}


int get_exit_status(int tid)
{
    st_twisthread * thread = _get_ref_byid(tid);
    if(thread != NULL)
    {
        if(thread->state == TERMINATE || thread->state == ABORT)
        {
            return thread->exit_status;
        }
        else
        {
            twisted_info("\n\tERROR: thread not terminated !!\n");
        }
    }
    twisted_info("\n\tERROR: thread id not there !!\n");    
    return -999;
}


/* _get_key_ref_byid() : to get the reference of the key specified by ID
 * @keyid : ID of the key
 * return : on success return reference of the key
 *          on failure return NULL
 *
 * It is a internal function which is used to get reference of a key by
 * its key ID
 */
static st_key * _get_key_ref_byid(int keyid){
 
    st_key * temp_key_ref = g_key_map.array;

    while(temp_key_ref != NULL){
        if(temp_key_ref->id == keyid){
            break;
        }
        temp_key_ref = temp_key_ref->next;
    }
    return temp_key_ref;        
}


/* get_key()  : To get a key for a specified lock
 * @lock_type : Type of the lock (Soft Lock | Hard Lock)
 *              Soft Lock (Safe Lock): Deadlock never happens
 *              Hard Lock : Deadlock may happen
 * @return:     In success return the key id for a specific lock
 *              In failure -1
 */              
int get_key(int lock_type) {

    st_key * newkey = NULL;          /* new key node */
    st_key * temp = g_key_map.array; /* start of the key list */   

    /* allocate memory for the key */    
    newkey = (st_key*)malloc(sizeof(st_key));
    /* check if memory allocation fails */
    if(newkey == NULL) {
        twisted_error("Malloc error !");
        return -1;
    }
    /* check if lock type if valid */
    if(lock_type != SOFT_LOCK && lock_type != HARD_LOCK){
        twisted_error("Invalid lock type !");
        return -1;
    }
    /* intialize key */
    newkey->thread_id = INVALID;
    newkey->lock_status = FALSE;
    newkey->lock_type = lock_type;
    /* ID starts from 1 */
    newkey->id = ++g_key_map.nos;        
    newkey->next = NULL;
    /* add it to key list */
    if(temp != NULL) {
       while(temp->next != NULL) {
           temp = temp->next;
       }
       temp->next = newkey;             
    }
    else {
       g_key_map.array = newkey;            
    }
    /* return key id */
    return newkey->id;
}


/* twist_lock(): lock a particuler lock with a specified key 
 * @key_id   : Key Id of the lock
 * @return   : On success return 0 (It may never return if deadlock)
 *             On failure return -1
 */
int twist_lock(int key_id) {

   st_key *ref_current = NULL;       /* reference of the key */ 
   st_twisthread *owner_thread = NULL; /* reference of the current thread owner */
   st_twisthread *current_thread = NULL;

   /* check if called from ISR */
   if(g_twisted_core.scheduler.ininterrupt) {
       twisted_error("Csn't be called from ISR !\n");
       return FAILURE;       
   }

   /* get the reference of the key by it's id */
   ref_current = _get_key_ref_byid(key_id);
   /* check if key_id is valid */   
   if(ref_current == NULL) {          
       twisted_error("Invalid Key id !!");    
       return FAILURE;  
   } 
  
   /* try to lock in loop until aquired (if already lock then yeild to execute other thread) */
   while(TRUE) {
       current_thread =  g_twisted_core.scheduler.current_thread; 
       /* if not already aquired then get it */
       if(ref_current ->lock_status == FALSE) {
           ref_current->lock_status = TRUE;
           ref_current->thread_id = twist_get_tid();   
           /* set lock owned info in thread */
	   current_thread->lock_owned = key_id; 
           return SUCCESS;    
       }
       /* if aquired then yeild so that owner thread could execute */
       else {
           twisted_info("Already locked by: %d",ref_current->thread_id);
           /* as high priority process gets executed, so if owner thread has 
           * lesser priority it never gets executed. To avoid the deadlock and
           * busy looping owner thread priority incremented one more than the 
           * current thread's priority in case if a SOFT_LOCK. 
           */
           if(ref_current->lock_type == SOFT_LOCK){
               /* get reference of the owner thread */
               owner_thread = _get_ref_byid(ref_current->thread_id);
               /* check if ref is valid */
               if(owner_thread == NULL){
                   twisted_critical("Lock owner thread ref not found");
                   exit(-1);
               }
               /* check if owner thread priority is lees than or equal of the current thread */
               if(owner_thread->priority <= g_twisted_core.scheduler.current_thread->priority){ 
                   printf("Current thread (%d) priority: %d, Owner thread (%d) priority: %d \n", 
                          g_twisted_core.scheduler.current_thread->id, g_twisted_core.scheduler.current_thread->priority, 
                                      owner_thread->id, owner_thread->priority);
                   /* set owner's priority one more than the current thread priotrity */
                   owner_thread->priority = g_twisted_core.scheduler.current_thread->priority;
               }
           }
           /* yeild to schedule other thread */
           twist_yeild();
       }
   }
   return FAILURE;
}


/* twist_unlock(): unlock a particuler lock (which was locked befor) with a 
 *               specified key
 * @key_id     : Id of the key
 * @return     : On success return 0
 *               On failure -1
 */ 
int twist_unlock(int key_id){

    st_key *ref = NULL;         /* reference of the key */
    st_twisthread *thread = NULL; /* reference of the owner thread */
    int current_tid = INVALID;  /* ID of the current thread */

    /* check if called from ISR */
    if(g_twisted_core.scheduler.ininterrupt) {
        twisted_error("Could not be called from ISR");
        return FAILURE;
    }
 
    /* get the reference of the key */
    ref = _get_key_ref_byid(key_id);
    /* check if Invalid key */       
    if(ref == NULL) {
        twisted_error("Invalid key id");
        return FAILURE;
    }
    
    /* get current thread id */
    current_tid = twist_get_tid();
    /* check if called from owner thread */    
    if(ref->thread_id != current_tid) {
        twisted_error("Owner thread should  unlock \n");
        return FAILURE;
    }
    
    /* check if lock is locked */
    if(ref->lock_status != TRUE) {
        twisted_error("Should be locked before unlock\n");
        return FAILURE;
    }

    /* get owner thread reference */
    thread = _get_ref_byid(ref->thread_id);
    /* restore to original priority of owner thread */
    thread->priority = thread->u_priority;
    /* Invalided lock owned information */
    g_twisted_core.scheduler.current_thread->lock_owned = INVALID;   
    ref->lock_status = FALSE;
    ref->thread_id = INVALID;        
    
    return SUCCESS;
}


/* print_ready_queue_() : prints the threads currently in thre readyqueue
 *                        i.e. in ready state
 * @return : On success returns the no of thread in ready state
 *           On failure return -1
 * 
 * Used for debugging purpose. In includes currently running thread also. 
 * So the count also includes the currently running thread
 */
int print_ready_queue_(void) {

    st_trefnode * temp_thread_ref = NULL;   /* temp reference of the thread ref node */
    st_twisthread * thread = NULL;

    /* get the first thread ref node */
    temp_thread_ref = g_twisted_core.ready_queue->start;
    /* check if core is initialzed */
    if(twist_init_ver == 0){
        twisted_error("twisted Core is unstable or not initialised");
        return FAILURE;
    }
    /* prints the thread info */ 
    printf("START -> ");    
    while(temp_thread_ref != NULL) {
        thread = (st_twisthread*)temp_thread_ref->data;
        printf(" ( %p| %d | %d ) ->", thread, thread->id, thread->priority);    
        temp_thread_ref = temp_thread_ref->next;
    }
    printf("END\n");
    /* return the nos of thread in ready queue */
    return g_twisted_core.ready_queue->nos;
}


#if 0

/* twist_create_interrupt(): Register a interrupt service routine to current 
 *                         thread. Should be called from a thread
 * @service_routine :   Function pointer of the service routine
 * @interrupt_id    :   The id of the interrupt 
 * @return          :   on success returns 0
 *                       on failure returns -1
 */   
int twist_create_interrupt(void (*service_routine)(int), int interrupt_id) {

    st_interrupt_thread *new_thread_node = NULL;  /* new thread info node */ 
    st_interrupt_thread *temp_thread_ref = NULL;  /* temp thread info ref */
    st_interrupt temp;                    /* temp node to hold interrupt node */
    int current_thread_id = twist_get_tid();        /* current thread id */
    
    /* check if valid interrupt id */
    if(interrupt_id < MAX_INTERRUPT && interrupt_id > 1){
        temp = g_interrupt_map.array[interrupt_id - 1];
    }    
    else {
        twisted_error("Wrong interrupt value");
        return FAILURE;
    }
    
    /* check thread list of specified interrupt id */
    temp_thread_ref = temp.list;
    /* check if no thread and service_routine is registered */
    if(temp_thread_ref != NULL){
        /* traverse untill last */
        while(TRUE){
            /* check if current thread is already registered */
            if(temp_thread_ref->thread_id == current_thread_id){
                twisted_info("replacing previous service routine by current");
                /* replace the service routine */
                temp_thread_ref->service_routine = service_routine;
                return SUCCESS;
            }
            /* check if not the last */ 
            if(NOT_LAST(temp_thread_ref)){
                temp_thread_ref = temp_thread_ref->next;
            }
            else 
                break;
        }
    }

    /* create new node and add details */
    new_thread_node = (st_interrupt_thread*)malloc(sizeof(st_interrupt_thread));
    if(new_thread_node == NULL) {        
        twisted_error("Malloc error");
        return FAILURE;
    }
    new_thread_node->thread_id = current_thread_id;
    new_thread_node->service_routine = service_routine;
    new_thread_node->next = NULL; 

    /* add node to the link list */
    if(temp_thread_ref == NULL){
        temp.list = new_thread_node;
    }
    else {
        temp_thread_ref->next = new_thread_node;
    }            
    
    /* incremant the associated thread nos for specified interrupt */
    temp.nos++;
  
    return SUCCESS;
}


int twist_raise_interrupt(int interrupt_id) {
  
}

/* twist_interrupt_thread() : to send specified interrupt to a specified thread
 * @interrupt_id : ID of the interrupt 
 * @thread_id    : ID of the thread
 * @return       : On success 0 is returned
 *                 On failure Error code is returned
 *
 * Call to provide interrupt sending mechanism. Interrupting a thread restwists 
 * an ISR to be called, if registered to the specified thread
 */
int twist_interrupt_thread(int interrupt_id, int thread_id) {
     
	st_twisthread *thread_ref = NULL;   /* original thread ref */
	st_twisthread *current_thread_ref = NULL; /* current thread ref */
	st_interrupt *interrupt_ref = NULL;   /* interrupt node ref */
	st_interrupt_thread *interrupt_thread_ref = NULL;  /* ISR registered thread ref */
	int poped_interrupt_ctid = INVALID;   /* poped interrupt called thread id */
	int flag_thread_found = 0;   /* registered hread found flag */
	int current_thread_id = INVALID;
	int lj_ret = 0;
	
	
	/* check if interrupt id is valid */
	if(interrupt_id > MAX_INTERRUPT || interrupt_id < IN_1) {
	    twisted_error("Invalid interrupt ID");
		return -E_INVAL_INTR_ID;
	}
	/* check if thread ref is valid */
	thread_ref = _get_ref_byid(thread_id);
	if(thread_ref == NULL) {
	    twisted_error("Invalid thread id"); 
		return -E_INVAL_THRD_ID;
	}
	/* Get the interrupt reference */
	interrupt_ref = g_interrupt_map.array[interrupt_id - 1];
	/* check if any ISR is registered forb the thread */
    /* get the interrupt thread node as first of the list*/
    interrupt_thread_ref = interrupt_ref.list;
    /* check if no thread and service_routine is registered */
    if(interrupt_thread_ref != NULL){
        /* traverse untill last */
        while(TRUE){
            /* check if current thread is already registered */
            if(interrupt_thread_ref->thread_id == current_thread_id){
                twisted_info("ISR is registered for interrupt <%u> and Thread Id <%u>", interrupt_id, thread_id);
                flag_thread_found = 1;
				break;
            }
            /* check if not the last */ 
            if(NOT_LAST(interrupt_thread_ref)){
                interrupt_thread_ref = temp_thread_ref->next;
            }
            else 
                break;
        }
    }
	/* check if thread is not found (No ISR was registered)*/
	if(!flag_thread_found) {
        twisted_info("Thread of ID <%u> received interrupt <%u> : undefined operation", thread_id, interrupt_id);
        return SUCCESS;
	}	
	else {
	    /* check if called from main */
		if(!g_twisted_core.scheduler.inthread) {		    
			/* push new interrupt node to pre_interrupt_stack.
		     * for main called_thread_id is passed as 0. */
			_push_interrupt_to_stack(interrupt_id, thread_id, 0);
			/* set the current_interrupt_id as current */
			g_interrupt_map.current_interrupt_id = interrupt_id;
			/* setting the in_service_routine flag of the interrupt thread ref */
			interrupt_thread_ref->in_service_routine = 1;
			/* save main context and continue */
			if(lj_ret = !setjmp(g_twisted_core.scheduler.main, 0)) {
				/* get original thread buf for context switching */
				/* set current_thread ref in scheduler */
				g_twisted_core.scheduler.current_thread = thread_ref;
				/* set inthread flag in scheduler */
				g_twisted_core.scheduler.inthread = 1;
				/* set ininterrupt flag in scheduler */
				g_twisted_core.scheduler.ininterrupt = 1;
				/* set state of the thread */
				thread_ref.state = INTERRUPT;
				/* ISR_LJ_RElTURN stats that the context is switched for 
				 * execution of ISR of the thread */
				longjmp(thread_ref->buf, ISR_LJ_RElTURN);
			}
			/* return for interrupt service routine execution */
			else if(lj_ret == ISR_LJ_RElTURN){
			    /*XXX*/
				
			}
			else {
				/* pop the top of the pre_interrupt_stack */
				poped_interrupt_ctid = _pop_interrupt_from_stack();
				/* check if pop fials */
				if(poped_interrupt_ctid == INVALID) {
				    twisted_critical("interrupt module error: pope failed in interrupt stack");
				    /* perform abort operation */
				    _twisted_abort();
				}
				/* check if stack is not empty */
                if(g_interrupt_map.pre_interrupt_stack.nos > 0 || g_interrupt_map.pre_interrupt_stack.top != NULL) {
                    twisted_critical("interrupt module error: main should be the last in stack");
				    /* perform abort operation */
				    _twisted_abort();
                }				
				/* unset the in_service_routine flag of the interrupt thread ref */
                interrupt_thread_ref->in_service_routine = 0;
                /* unset the current_interrupt_id as returned from main */
				g_interrupt_map.current_interrupt_id = 0;
				/* it should done on the terget thread side */
				/* XXX */
					/* unset inthread flag in scheduler as returning to main */
					g_twisted_core.scheduler.inthread = 0;
					/* unset ininterrupt flag in scheduler (as main should be the last) */
					g_twisted_core.scheduler.ininterrupt = 0;
					/* set current thread as NULL */
					g_twisted_core.scheduler.current_thread = NULL;
				/* XXX */
                /* check yeild flag for performing context switching */
                /* XXX */				
			}			
		}
		/* called from another thread */
		else {
		    current_thread_id = twist_get_tid();
		    /* check if thread_id is same as self id */
			if(thread_id == current_thread_id) {
			    /* push new interrupt node to pre_interrupt_stack.
				 * for self called_thread_id is passed as current_thread_is. */
				_push_interrupt_to_stack(interrupt_id, thread_id, current_thread_id);
				/* set the in_service_routine flag of the interrupt thread ref */
				interrupt_thread_ref->in_service_routine = 1;
				/* save the current state of the tergeted thread */
				interrupt_thread_ref.t_state = interrupt_thread_ref.state;
				/* set the state of tergeted thread */
				interrupt_thread_ref.state = INTERRUPT;
				/* set ininterrupt flag in scheduler */
				g_twisted_core.scheduler.ininterrupt = 1;
				/* call ISR of the current thread */
                interrupt_thread_ref->service_routine(interrupt_id);
				/* unset ininterrupt flag in scheduler */
				g_twisted_core.scheduler.ininterrupt = 1;
				/* restore the state of tergeted thread */
				interrupt_thread_ref.state = interrupt_thread_ref.t_state;
				interrupt_thread_ref.t_state = INVALID;
				/* unset the in_service_routine flag of the interrupt thread ref */
				interrupt_thread_ref->in_service_routine = 0;
				/* pop the top of the pre_interrupt_stack */
                _pop_interrupt_from_stack();		
			}
			else {
				/* push new interrupt node to pre_interrupt_stack.
				 * for self called_thread_id is passed as current_thread_is. */
				_push_interrupt_to_stack(interrupt_id, thread_id, current_thread_id);
				/* set the current_interrupt_id as current */
				g_interrupt_map.current_interrupt_id = interrupt_id;
				/* setting the in_service_routine flag of the interrupt thread ref */
				interrupt_thread_ref->in_service_routine = 1;
				/* get current thread ref by id */
				current_thread_ref = _get_ref_byid(current_thread_id);
				/* save current thread context and continue */
				if(!setjmp(current_thread_ref->buf, 0)) {
					/* get original thread buf for context switching */
					/* set current_thread ref in scheduler */
					g_twisted_core.scheduler.current_thread = thread_ref;
					/* set inthread flag in scheduler */
					g_twisted_core.scheduler.inthread = 1;
					/* set ininterrupt flag in scheduler */
					g_twisted_core.scheduler.ininterrupt = 1;
					/* set state of the thread */
					thread_ref.state = INTERRUPT;
					/* ISR_LJ_RElTURN stats that the context is switched for 
					 * execution of ISR of the thread */
					longjmp(thread_ref->buf, ISR_LJ_RElTURN);
				cheduler
				else {
					/* pop the top of the pre_interrupt_stack */
					poped_interrupt_ctid = _pop_interrupt_from_stack();
					/* check if pop fails */
					if(poped_interrupt_ctid == INVALID) {
						twisted_critical("interrupt module error: pope failed in interrupt stack");
						/* perform abort operation */
						_twisted_abort();
					}
					/* check if poped the current thread */
					if(poped_interrupt_ctid != current_thread_id) {
					    twisted_critical("interrupt module error: pope thread id not matched with current thread id");
						/* perform abort operation */
						_twisted_abort();
					}
					/* check if stack is empty */
					if(g_interrupt_map.pre_interrupt_stack.nos == 0 && g_interrupt_map.pre_interrupt_stack.top == NULL) {
						/* unset ininterrupt flag in scheduler */
						g_twisted_core.scheduler.ininterrupt = 0;
						/* it should done on the terget thread side */
						g_interrupt_map.current_interrupt_id = 0;
					}
                    else {
						/* unset ininterrupt flag in scheduler */
						g_twisted_core.scheduler.ininterrupt = 1;
						/* it should done on the terget thread side */
						g_interrupt_map.current_interrupt_id = /*<get from current stack top>*/;
					}
					/* unset the in_service_routine flag of the interrupt thread ref */
					interrupt_thread_ref->in_service_routine = 0;
					/* check yeild flag for performing context switching */
					/* XXX */				
				}
			}
		}
	}
}

#endif

/* twist_scheduler() : to select scheduler type on runtime 
 * @type : Type of the scheduler
 * @return : On success returns 0
 *           On failure returns ERROR CODE
 *
 * selecting different scheduler type actually changes the scheduler type at 
 * runtime. The defatwist scheduling type is non preeemptive priority scheduling 
 * (NPPRIORITY). There is basically three types of scheduler is available:
 * 1> FCFS scheduling (FCFS)
 * 2> Non Preemptive priority scheduling (NPPRIORITY => FCFS | PRIORITY)
 * 3> Round Robin Scheduling (RR => FCFS | RR)
 * 4> Round Robin Priority Scheduling (PRIORITYRR => PRIORITY | RR)
 */
int twist_scheduler(int type) {

    int current_stype = g_twisted_core.scheduler.type; /* current scheduler type */
    int c_id = getpid();
    int clock_pid = getpid();
    void *clock_pstack = NULL;
    struct sigaction handler;
   
    /* check scheduler type and take action acccordingly */
    switch(type) {
	    /*
        case RR: 
        if(current_stype == RR){
            twisted_info("\n\ttype not chenged\n");
            return SUCCESS;
        }
        else {
            handler.sa_handler = &_RR_schedule_handler;
            handler.sa_flags = 0;
            sigemptyset(&handler.sa_mask);
            sigaction(twisted_SIGNAL, &handler, NULL);
            clock_pstack = malloc(CLOCK_STACK_SIZE);
            if(clock_pstack == NULL) {
                twisted_error("Clock stack creation failed\n");
                return -ESCH_STACKC;
            }
            //clock_pid = clone(_RRscheduler_clock, clock_pstack, CLONE_VM, NULL);
            if(clock_pid <= 0) {
                twisted_error("clone failed - continuing with prev scheduler\n"); 
                free(clock_pstack);
                return -ESCH_CLONEF;
            }
            g_twisted_core.scheduler.type = RR;
            g_twisted_core.scheduler.clock_pid = clock_pid;            
            g_twisted_core.scheduler.clock_pstack = clock_pstack;            
        }
        break;*/

        case FCFS:
        if(current_stype == FCFS){
            twisted_info("\n\ttype not chenged\n");
            return SUCCESS;
        }
        else {

 
        }
        break;
/*
        case PRIORITY:
        if(current_stype == NPPRIORITY){
            twisted_info("\n\ttype not chenged\n");
            return SUCCESS;
        }
        else {

 
        }
        break
 
        case PRIORITYRR:
        if(current_stype == PRIOTITYRR){
            twisted_info("\n\ttype not chenged\n");
            return SUCCESS;
        }
        else {

 
        }
        break;
*/    
        defatwist:
        twisted_info("\n\tUnknown Type\n");
        return FAILURE;
        
    }
    return 0;
}

/*
int twist_free(int tid)
{
    st_twisthread * thread = _get_ref_byid(tid);
    st_trefnode * tnode;
    st_trefnode * tpnode = NULL;
    st_trefnode * temp = g_twisted_core.thread_ref_map.array;    
    st_trefnode * prev = NULL;    
    st_trefnode * freenode;
    st_event * enode;
    if(thread != NULL)
    {
        if(thread->state == NEW || thread->state == TERMINATE || thread->state == ABORT)
        {
            while(temp!= NULL)
            {    
		 if(((st_twisthread*)temp->data) == thread)
		 {
		       if(prev == NULL) {
			    freenode = temp;                
			    g_twisted_core.thread_ref_map.array = temp->next;
			    thread = (st_twisthread*)freenode->data;
			    free(thread->stack.ss_sp);
			    free(thread);                
			    free(freenode);                
			    return 0;
		       }
		       else {
			    freenode = temp;
			    prev->next = temp->next;
			    thread = (st_twisthread*)freenode->data;
			    free(thread->stack.ss_sp);
			    free(thread);                
			    free(freenode);                
			    return 0;                    
                       }
                 }
                 prev = temp;
                 temp = temp->next;            
            }
            return -1;
        }
        else if(thread->state == READY)
        {
            if(QUEUE_SIZE(g_twisted_core.ready_queue) > 1)
            {
                if(thread->prev == NULL)
                {
			g_twisted_core.ready_queue.start = thread->next;
			g_twisted_core.ready_queue.start->prev = NULL;
                }
                else if(thread->next == NULL)
                {
			thread->prev->next = NULL;
			g_twisted_core.ready_queue.end = thread->prev;
                }
                else
                {
			thread->prev->next = thread->next;
			thread->next->prev = thread->prev;
                }
                g_twisted_core.ready_queue.nos--;
            }
            else
            {
                g_twisted_core.ready_queue.start = NULL;
                g_twisted_core.ready_queue.end = NULL;
                g_twisted_core.ready_queue.nos = 0;    
            }
            while(temp!= NULL)
            {    
		    if(temp->thread == thread)
		    {
			if(prev == NULL)
			{
			    freenode = temp;                
			    g_twisted_core.thread_ref_map.array = temp->next;
			    free(freenode->thread->stack.ss_sp);
			    free(freenode->thread);                
			    free(freenode);                
			    return 0;
			}
			else
			{
			    freenode = temp;
			    prev->next = temp->next;
			    free(freenode->thread->stack.ss_sp);
			    free(freenode->thread);
			    free(freenode);
			    return 0;                    
			}
		    }
            	    prev = temp;
                    temp = temp->next;            
            }
	    return -1;
        }
        else if(thread->state==WAITING)
        {
            enode = g_event_map.list;  
            while(enode!=NULL)
            {
		    tnode = enode->array;
		    while(tnode != NULL)
		    {    
			if(tnode->thread == thread)
			{
			    if(tpnode == NULL)
			    {
				enode->array = tnode->next;
				free(tnode);                    
			    }
			    else
			    {
				tpnode->next = tnode->next;                
				free(tnode);                    
			    }
			    enode->nos--;
			}
			tpnode = tnode;
			   tnode = tnode->next;            
		    }
            }
            while(temp != NULL)
            {    
		    if(temp->thread == thread)
		    {
			if(prev == NULL)
			{
			    freenode = temp;                
			    g_twisted_core.thread_ref_map.array = temp->next;
			    free(freenode->thread->stack.ss_sp);                
			    free(freenode->thread);                
			    free(freenode);                
			    return 0;
			}
			else
			{
			    freenode = temp;
			    prev->next = temp->next;
			    free(freenode->thread->stack.ss_sp);
			    free(freenode->thread);
			    free(freenode);
			    return 0;                    
			}
		    }
		    prev = temp;
		    temp = temp->next;            
            }
            return -1;
        }
        else if(thread->state == RUNNING)
        {
                twisted_error("\n\tError: Running Thread can't be freed\n");
                return 1;
        }
    }
    else
    {
        twisted_info("\n\tERROR: thread id not there !!\n");    
    }
    return -1;    
}*/
