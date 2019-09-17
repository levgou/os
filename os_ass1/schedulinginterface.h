#pragma once

#include "types.h"

struct proc;

/**** The implementation of this is found in the ass1ds.cpp file. ****/

//The following c-structs are holding pointers to functions as fields, to make the usage
//easier, like in java.
//For example, suppose we have an instance "pq" of type "PriorityQueue", to invoke the
//isEmpty method just write:
//  boolean ans = pq.isEmpty();

//This structure holds the RUNNABLE processes - Policies 2 & 3
typedef struct PriorityQueue {
	//Checks whether this queue is empty
	boolean (*isEmpty)();

	//This function puts the given process to the priority queue.
	//It returns true if the operation succeeds. This operation may fail if you didn't
	//manage the structures correctly.
	boolean (*put)(struct proc* p);

	//Stores the value of the minimum accumulator inside the given accumulator pointer.
	//Returns true iff the queue isn't empty
	boolean (*getMinAccumulator)(long long* accumulator);

	//Extract a process with the minimum accumulator from the priority queue.
	//If this queue is empty it returns null.
	struct proc* (*extractMin)();

	//Call this function when you need to switch between policies.
	//This function transfers all the mapped process to the RoundRobinQueue.
	//It returns true if the operation succeeds. This operation may fail if you didn't
	//manage the structures correctly.
	boolean (*switchToRoundRobinPolicy)();

	//Extracts a specific process from the queue.
	//Use this function in policy 3 (Extended priority) once every 100 time quanta.
	//This function returns true if it succeeded to extract the given process,
	//it may fail if you didn't manage the data structures correctly.
	boolean (*extractProc)(struct proc* p);
} PriorityQueue;




//This structure holds the RUNNABLE processes - Policy 1
typedef struct RoundRobinQueue {
	//Checks whether this queue is empty.
	boolean (*isEmpty)();

	//Enqueue the given process to the queue - FIFO manner.
	//It returns true if the operation succeeds. This operation may fail if you didn't
	//manage the structures correctly.
	boolean (*enqueue)(struct proc* p);
	
	//Removes the first process from the queue and returns it - FIFO manner.
	//If the queue is empty it returns null.
	struct proc* (*dequeue)();
	
	//Call this function when you need to switch between policies.
	//This function transfers all the mapped process to the PriorityQueue.
	//It returns true if the operation succeeded. This operation may fail if you didn't
	//manage the structures correctly.
	boolean (*switchToPriorityQueuePolicy)();
} RoundRobinQueue;


//This structure holds the RUNNING processes
typedef struct RunningProcessesHolder {
	//Checks whether this structure is empty.
	boolean (*isEmpty)();

	//Adds a process to the structure.
	//It returns true if the operation succeeded. This operation may fail if you didn't
	//manage the structures correctly.
	boolean (*add)(struct proc* p);

	//Removes the given process from this structure.
	//Returns true iff the process was in the structure.
	boolean (*remove)(struct proc* p);

	//Stores the value of the minimum accumulator inside the given accumulator pointer.
	//Returns true iff the structure isn't empty.
	boolean (*getMinAccumulator)(long long *accumulator);
} RunningProcessesHolder;
