/*
Consider a classic livelock scenario in which a husband and wife named Alice and Bob
attempt to eat soup but only have one spoon between them. Each spouse is overly
courteous and will pass the spoon to the other if the other hasn’t eaten yet.

Use the steps below to create this livelock scenario in Golang:

Create a function that creates a lock on resource x (a spoon in this case),
does some processing (use the time.Sleep function for the time being), and before using
the resource, checks if it is required by the other process (the spouse in this case).

If the spouse is hungry, leave the lock on the resource (the spoon) and start demanding
the resources again. Launch the two goroutines that are passing WaitGroup and the
resource’s pointer reference.

For the output, try to print something that indicates that spouse one is picking up resource x,
checking if the other spouse is hungry, and leaving the resource.
*/


package main

import (
	"fmt"
	"runtime"
	"sync"
)

func main() {
	runtime.GOMAXPROCS(4)
	type value struct {
		sync.Mutex
		id     string
		value1 string
		value2 string
		locked bool
	}

	lock := func(v *value) {
		v.Lock()
		v.locked = true
	}
	unlock := func(v *value) {
		v.Unlock()
		v.locked = false
	}

	move := func(wg *sync.WaitGroup, v1, v2 *value) {
		defer wg.Done()

		for i := 0; ; i++ {
			if i > 3 {
				fmt.Println("canceling goroutine...")
				return
			}

			fmt.Printf("%v: %v \n", v1.id, v1.value1)
			lock(v1)

			fmt.Printf("%v: Checking if %v \n", v1.id, v2.value2)
			if v2.locked {
				fmt.Printf("%v: Leaving spoon \n", v1.id)
				unlock(v1)
				continue
			}
		}
	}

	alice, bob := value{
		id:     "alice",
		value1: "i am picking up the spoon",
		value2: "alice is hungry",
	}, value{
		id:     "bob",
		value1: "i am picking up the spoon",
		value2: "bob is hungry",
	}

	var wg sync.WaitGroup
	wg.Add(2)

	go move(&wg, &alice, &bob)
	go move(&wg, &bob, &alice)

	wg.Wait()
}
