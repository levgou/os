#pragma once

extern "C" {
	#include "types.h"
	#include "param.h"
	#include "schedulinginterface.h"
	void initSchedDS();
}

typedef struct proc Proc;

class Link;
class MapNode;
class LinkedList;
class Map;

static Link* allocLink(Proc *p);
static void deallocLink(Link *link);
static void deallocNode(MapNode *node);
static MapNode* allocNode(long long key);
static MapNode* allocNode(Proc *p, long long key);

class Link {
public:
	Link(): p(null), next(null) {}
	~Link() {}

private:
	//MARK: make some friends
	friend void initSchedDS();
	friend Link* allocLink(Proc *p);
	friend void deallocLink(Link *link);
	friend LinkedList;
	
	//MARK: private methods
	Link* getLast(); //returns the last link in this list

	//MARK: fields
	Proc *p;
	Link *next;
};

class LinkedList {
public:
	LinkedList(): first(null), last(null) {} 
	~LinkedList() {} 

	bool isEmpty(); //checks whether this linked list is empty

	bool enqueue(Proc* p); //append the given proc to the end of the list. Allocates a link node. Returns false if the allocation falied.
	Proc* dequeue(); //removes and returns the first proc of this linked list. Deallocates a link node. Returns null if this list is empty(). 
	
	bool remove(Proc *p); //remove a specific proc from this list. Returns true iff succeeds.

	bool transfer(); //transfers all the procs to the Priority Queue. Fails if allocations failed. Deallocates link nodes.
	bool getMinKey(long long *pkey); //stores the minimum key in the pkey arg. Returns true iff this list isn't empty.

private:
	//MARK: private methods
	void append(Link *link); //appends the given list to the queue. No allocations always succeeds.
	
	template<typename Func>
	void forEach(const Func& accept) { //for-each loop. gets a function that applies the procin each link node.
		Link *link = first;
		while(link) {
			accept(link->p);
			link = link->next;
		}
	}

	//MARK: fields
	Link *first, *last;
};

class MapNode {
public:
	MapNode(): listOfProcs(), next(null), parent(null), left(null), right(null) {}
	~MapNode() {}

	bool isEmpty(); //checks whether this->listOfProcs is empty
	bool put(Proc *p); //puts the give proc in the rooted tree. Allocates a map node if needed. Allocates a link node. Returns true iff succeeds.
	MapNode* getMinNode(); //returns the left most node of this rooted tree.
	void getMinKey(long long *pkey); //stores the minmum key of this rooted tree in the pkey arg.
	Proc* dequeue(); //removes and returns the first proc of this->listOfProcs. Deallocates a link node. Returns null if this->listOfProcs is empty(). 

private:
	//MARK: make some friends
	friend void initSchedDS();
	friend void deallocNode(MapNode *node);
	friend MapNode* allocNode(long long key);
	friend MapNode* allocNode(Proc *p, long long key);
	friend LinkedList;
	friend Map;

	//MARK: fields
	long long key;
	LinkedList listOfProcs;
	MapNode *next, *parent, *left, *right;
};

class Map {
public:
	Map(): root(null) {}
	~Map() {}

	bool isEmpty(); //checks whether this map is empty
	bool put(Proc *p); //puts the give proc in this->root node. Allocates a map node if needed. Allocates a link node. Returns true iff succeeds.
	bool getMinKey(long long *pkey); //stores the minmum key of this rooted tree in the pkey arg. Returns true iff this map isn't empty.
	Proc* extractMin(); //removes and returns a minimum proc from this map. Deallocates a map node if needed. Deallocates a link node. Returns null if this map is empty().
	bool transfer(); //transfers all the procs to the Round Robin Queue. Fails if allocations failed. Deallocates map nodes. Deallocates link nodes.
	bool extractProc(Proc *p); //remove a specific proc from this map. Returns true iff succeeds.

private:
	//MARK: make some friends
	friend LinkedList;

	//MARK: fields
	MapNode *root;
};
