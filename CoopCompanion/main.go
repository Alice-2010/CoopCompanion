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
	go handleMessages(closing)

	terminate := make(chan os.Signal, 1)
	signal.Notify(terminate, os.Interrupt)
	<-terminate

	fmt.Println("Shutting down...")
	// send closing signal then wait for confirmation before exiting
	closing <- true
	<-closing
}

func handleMessages(closing chan bool) {
	fmt.Println("Listening for UDP packets.")
	for {
		select {
		case <-closing:
			fmt.Println("Stopping UDP listener")
			closing <- true
			return
		default:
		}

		session, player, data := network.SocketRecv()
		if session == nil || player == nil || data == nil {
			continue
		}

		// Forward the data to the other client
		_, otherPlayer := network.GetClientFunc(*session, func(c network.SocketClient) bool {
			return c.PlayerType != player.PlayerType
		})
		if otherPlayer == nil {
			fmt.Printf("No other player to forward data to for session %s\n", session.ID)
			continue
		}
		network.SocketSend(otherPlayer, data)
	}
}
