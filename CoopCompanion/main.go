package main

import (
	"alice-coop-companion/network"
	"fmt"
	"os"
	"os/signal"
)

func main() {
	closing := make(chan bool)
	network.SocketListen()
	defer network.SocketClose()
	go network.SocketRecv(closing)

	terminate := make(chan os.Signal, 1)
	signal.Notify(terminate, os.Interrupt)
	<-terminate

	fmt.Println("Shutting down...")
	// send closing signal then wait for confirmation before exiting
	closing <- true
	<-closing
}
