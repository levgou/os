#include "ass1ds.hpp"

extern "C" {
char*                         kalloc();
void                          panic(char*) __attribute__((noreturn));
void*                         memset(void*, int, uint);
void                          initSchedDS();
long long                     getAccumulator(Proc *p);
long long                     __moddi3(long long number, long long divisor);

//for pq
static boolean                isEmptyPriorityQueue();
static boolean                putPriorityQueue(Proc* p);
static boolean                getMinAccumulatorPriorityQueue(long long* pkey);
static Proc*                  extractMinPriorityQueue();
static boolean                switchToRoundRobinPolicyPriorityQueue();
static boolean                extractProcPriorityQueue(Proc *p);

//for rrq
static boolean                isEmptyRoundRobinQueue();
static boolean                enqueueRoundRobinQueue(Proc *p);
static Proc*                  dequeueRoundRobinQueue();
static boolean                switchToPriorityQueuePolicyRoundRobinQueue();

//for rpholder
static boolean                isEmptyRunningProcessHolder();
static boolean                addRunningProcessHolder(Proc* p);
static boolean                removeRunningProcessHolder(Proc* p);
static boolean                getMinAccumulatorRunningProcessHolder(long long *pkey);

extern PriorityQueue          pq;
extern RoundRobinQueue        rrq;
extern RunningProcessesHolder rpholder;

PriorityQueue                 pq;
RoundRobinQueue               rrq;
RunningProcessesHolder        rpholder;
}

#define PGSIZE                    4096
#define NPROCLIST                 (2*NPROC) //take some extra space
#define NPROCMAP                  (2*NPROC) //take some extra space

static Map                        *priorityQ;
static LinkedList                 *roundRobinQ;
static LinkedList                 *runningProcHolder;

static Link                       *freeLinks;
static MapNode                    *freeNodes;

static char                       *data;
static uint                       spaceLeft;

static char* mymalloc(uint size) {
	if(spaceLeft < size) {
		data = kalloc();
		memset(data, 0, PGSIZE);
		spaceLeft = PGSIZE;
	}

	char* ans = data;
	data += size;
	spaceLeft -= size;
	return ans;
}

//for pq
static boolean isEmptyPriorityQueue() {
	return priorityQ->isEmpty();
}

static boolean putPriorityQueue(Proc* p) {
	return priorityQ->put(p);
}

static boolean getMinAccumulatorPriorityQueue(long long* pkey) {
	return priorityQ->getMinKey(pkey);
}

static Proc* extractMinPriorityQueue() {
	return priorityQ->extractMin();
}

static boolean switchToRoundRobinPolicyPriorityQueue() {
	return priorityQ->transfer();
}

static boolean extractProcPriorityQueue(Proc *p) {
	return priorityQ->extractProc(p);
}

//for rrq
static boolean isEmptyRoundRobinQueue() {
	return roundRobinQ->isEmpty();
}

static boolean enqueueRoundRobinQueue(Proc *p) {
	return roundRobinQ->enqueue(p);
}

static Proc* dequeueRoundRobinQueue() {
	return roundRobinQ->dequeue();
}

static boolean switchToPriorityQueuePolicyRoundRobinQueue() {
	return roundRobinQ->transfer();
}

//for rpholder
static boolean isEmptyRunningProcessHolder() {
	return runningProcHolder->isEmpty();
}

static boolean addRunningProcessHolder(Proc* p) {
	return runningProcHolder->enqueue(p);
}

static boolean removeRunningProcessHolder(Proc* p) {
	return runningProcHolder->remove(p);
}

static boolean getMinAccumulatorRunningProcessHolder(long long *pkey) {
	return runningProcHolder->getMinKey(pkey);
}

void initSchedDS() { //called once by the "pioneer" cpu from the main function in main.c
	data               = null;
	spaceLeft          = 0u;

	priorityQ          = (Map*)mymalloc(sizeof(Map));
	*priorityQ         = Map();

	roundRobinQ        = (LinkedList*)mymalloc(sizeof(LinkedList));
	*roundRobinQ       = LinkedList();

	runningProcHolder  = (LinkedList*)mymalloc(sizeof(LinkedList));
	*runningProcHolder = LinkedList();

	freeLinks = null;
	for(int i = 0; i < NPROCLIST; ++i) {
		Link *link = (Link*)mymalloc(sizeof(Link));
		*link = Link();
		link->next = freeLinks;
		freeLinks = link;
	}

	freeNodes = null;
	for(int i = 0; i < NPROCMAP; ++i) {
		MapNode *node = (MapNode*)mymalloc(sizeof(MapNode));
		*node = MapNode();
		node->next = freeNodes;
		freeNodes = node;
	}

	//init pq
	pq.isEmpty                      = isEmptyPriorityQueue;
	pq.put                          = putPriorityQueue;
	pq.getMinAccumulator            = getMinAccumulatorPriorityQueue;
	pq.extractMin                   = extractMinPriorityQueue;
	pq.switchToRoundRobinPolicy     = switchToRoundRobinPolicyPriorityQueue;
	pq.extractProc                  = extractProcPriorityQueue;

	//init rrq
	rrq.isEmpty                     = isEmptyRoundRobinQueue;
	rrq.enqueue                     = enqueueRoundRobinQueue;
	rrq.dequeue                     = dequeueRoundRobinQueue;
	rrq.switchToPriorityQueuePolicy = switchToPriorityQueuePolicyRoundRobinQueue;

	//init rpholder
	rpholder.isEmpty                = isEmptyRunningProcessHolder;
	rpholder.add                    = addRunningProcessHolder;
	rpholder.remove                 = removeRunningProcessHolder;
	rpholder.getMinAccumulator      = getMinAccumulatorRunningProcessHolder;
}

static Link* allocLink(Proc *p) {
	if(!freeLinks)
		return null;

	Link *ans = freeLinks;
	freeLinks = freeLinks->next;
	ans->next = null;
	ans->p = p;
	return ans;
}

static void deallocLink(Link *link) {
	link->next = freeLinks;
	freeLinks = link;
}

static void deallocNode(MapNode *node) {
	node->parent = node->left = node->right = null;
	node->next = freeNodes;
	freeNodes = node;
}

static MapNode* allocNode(long long key) {
	if(!freeNodes)
		return null;

	MapNode *ans = freeNodes;
	freeNodes = freeNodes->next;
	ans->next = null;
	ans->key = key;
	return ans;
}

static MapNode* allocNode(Proc *p, long long key) {
	if(!freeNodes)
		return null;

	MapNode *ans = allocNode(key);
	if(!ans)
		return null;

	if(!ans->listOfProcs.enqueue(p)) {
		deallocNode(ans);
		return null;
	}

	return ans;
}

Link* Link::getLast() {
	Link* ans = this;

	while(ans->next)
		ans = ans->next;

	return ans;
}

bool LinkedList::isEmpty() {
	return !first;
}

void LinkedList::append(Link *link) {
	if(!link)
		return;

	if(isEmpty()) first = link;
	else last->next = link;

	last = link->getLast();
}

bool LinkedList::enqueue(Proc *p) {
	Link *link = allocLink(p);

	if(!link)
		return false;

	append(link);
	return true;
}

Proc* LinkedList::dequeue() {
	if(isEmpty())
		return null;

	Proc *p = first->p;
	Link *next = first->next;

	deallocLink(first);

	first = next;

	if(isEmpty())
		last = null;

	return p;
}

bool LinkedList::remove(Proc *p) {
	if(isEmpty())
		return false;

	if(first->p == p) {
		dequeue();
		return true;
	}

	Link *prev = first;
	Link *cur = first->next;
	while(cur) {
		if(cur->p == p) {
			prev->next = cur->next;

			if(!(cur->next)) //removes the last link
				last = prev;

			deallocLink(cur);

			return true;
		}

		prev = cur;
		cur = cur->next;
	}

	//didn't find the process
	return false;
}

bool LinkedList::transfer() {
	if(!priorityQ->isEmpty())
		return false;

	if(!isEmpty()) {
		MapNode *node = allocNode(0);
		if(!node)
			return false;

		node->listOfProcs.first = first;
		node->listOfProcs.last = last;
		first = last = null;
		priorityQ->root = node;
	}
	return true;
}

bool LinkedList::getMinKey(long long *pkey) {
	if(isEmpty())
		return false;

	long long minKey = getAccumulator(first->p);

	forEach([&](Proc *p) {
		long long key = getAccumulator(p);
		if(key < minKey)
			minKey = key;
	});

	*pkey = minKey;

	return true;
}

bool MapNode::isEmpty() {
	return listOfProcs.isEmpty();
}

bool MapNode::put(Proc *p) { //we can not use recursion, since the stack of xv6 is too small....
	MapNode *node = this;
	long long key = getAccumulator(p);
	for(;;) {
		if(key == node->key)
			return node->listOfProcs.enqueue(p);
		else if(key < node->key) { //left
			if(node->left)
				node = node->left;
			else {
				node->left = allocNode(p, key);
				if(node->left) {
					node->left->parent = node;
					return true;
				}
				return false;
			}
		} else { //right
			if(node->right)
				node = node->right;
			else {
				node->right = allocNode(p, key);
				if(node->right) {
					node->right->parent = node;
					return true;
				}
				return false;
			}
		}
	}
}

MapNode* MapNode::getMinNode() { //no recursion.
	MapNode* minNode = this;
	while(minNode->left)
		minNode = minNode->left;

	return minNode;
}

void MapNode::getMinKey(long long *pkey) {
	*pkey = getMinNode()->key;
}

Proc* MapNode::dequeue() {
	return listOfProcs.dequeue();
}

bool Map::isEmpty() {
	return !root;
}

bool Map::put(Proc *p) {
	long long key = getAccumulator(p);
	if(isEmpty()) {
		root = allocNode(p, key);
		return !isEmpty();
	}

	return root->put(p);
}

bool Map::getMinKey(long long *pkey) {
	if(isEmpty())
		return false;

	root->getMinKey(pkey);
	return true;
}

Proc* Map::extractMin() {
	if(isEmpty())
		return null;

	MapNode *minNode = root->getMinNode();

	Proc *p = minNode->dequeue();

	if(minNode->isEmpty()) {
		if(minNode == root) {
			root = minNode->right;
			if(!isEmpty())
				root->parent = null;
		} else {
			MapNode *parent = minNode->parent;
			parent->left = minNode->right;
			if(minNode->right)
				minNode->right->parent = parent;
		}
		deallocNode(minNode);
	}

	return p;
}

bool Map::transfer() {
	if(!roundRobinQ->isEmpty())
		return false;

	while(!isEmpty()) {
		Proc* p = extractMin();
		roundRobinQ->enqueue(p); //should succeed.
	}

	return true;
}

bool Map::extractProc(Proc *p) {
	if(!freeNodes)
		return false;

	bool ans = false;
	Map tempMap;
	while(!isEmpty()) {
		Proc *otherP = extractMin();
		if(otherP != p)
			tempMap.put(otherP); //should scucceed.
		else ans = true;
	}
	root = tempMap.root;
	return ans;
}

long long __moddi3(long long number, long long divisor) { //returns number%divisor
	if(divisor == 0)
		panic((char*)"divide by zero!!!\n");

	bool isNumberNegative = false;
	if(number < 0) {
		number = -number;
		isNumberNegative = true;
	}

	if(divisor < 0)
		divisor = -divisor;

	for(;;) {
		long long divisor2 = divisor;
		while(number >= divisor2) {
			number -= divisor2;
			if(divisor2 + divisor2 > 0) //exponential decay.
				divisor2 += divisor2;
		}

		if(number < divisor)
			return isNumberNegative ? -number : number;
	}
}