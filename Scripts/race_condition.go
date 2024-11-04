package main

import "time"

// this code triggers race condition

// write function to read value
func read(shared_map map[int]int) {
	for {
		var _ = shared_map[0]
	}
}

// write function to update value
func update(shared_map map[int]int) {
	for {
		shared_map[0] = shared_map[0] + 1
	}
}

// complete the main function as well.
func main() {
	shared_map := make(map[int]int)
	shared_map[0] = 0

	go read(shared_map)
	go update(shared_map)

	time.Sleep(2 * time.Second)
}
