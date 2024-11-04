package main

import (
	"fmt"
	"sync"
	"time"
)

// Note to self :In the critical section, keep only the important things.
// Take the rest of the code out of the lock.

func readMap(sharedMap map[int]int, mu *sync.Mutex) {
	for {
		// The lock is mandatory so that only one goroutine can access the critical section at a time.
		// The critical section in our code occurs when the value is read and updated.
		mu.Lock()
		val := sharedMap[0]
		fmt.Println(val)
		mu.Unlock()
	}
}

func write(sharedMap map[int]int, mu *sync.Mutex) {
	for {
		// Lock while updating the value.
		mu.Lock()
		sharedMap[0]++
		mu.Unlock()
	}
}

func main() {
	var mu sync.Mutex
	sharedMap := make(map[int]int)
	sharedMap[0] = 0

	// since goroutines will trigger randomly, the numbers printed may not be in +1 increment fashion
	go readMap(sharedMap, &mu)
	go write(sharedMap, &mu)
	// change time to see difference
	time.Sleep(10 * time.Millisecond)
}