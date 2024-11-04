package main

import (
	"fmt"
	"os"
	"strconv"
)

// Function to generate Fibonacci numbers
func fibonacci(n int) []int {
	fibSeq := make([]int, n)
	if n > 0 {
		fibSeq[0] = 0
	}
	if n > 1 {
		fibSeq[1] = 1
	}
	for i := 2; i < n; i++ {
		fibSeq[i] = fibSeq[i-1] + fibSeq[i-2]
	}
	return fibSeq
}

// Function to be run by the goroutine
func generateFibonacci(num int) chan []int {
	resultChannel := make(chan []int)
	go func() {
		fibSeq := fibonacci(num)
		resultChannel <- fibSeq
	}()
	return resultChannel
}

func main() {

	num, err := strconv.Atoi(os.Args[1])
	if err != nil || num <= 0 {
		fmt.Println("Please enter a positive integer.")
		return
	}

	// Generate Fibonacci numbers in a goroutine
	resultChannel := generateFibonacci(num)

	// Wait for the goroutine to finish and get the result
	fibSeq := <-resultChannel

	// Output the generated Fibonacci sequence
	fmt.Println(fibSeq)
}
