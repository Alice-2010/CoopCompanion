package main

import (
	"alice-coop-companion/config"
	"alice-coop-companion/network"
	"fmt"
	"os"
	"os/signal"
)

func main() {
	logger := config.GetLogger()
	cfg, err := config.ReadConfig()
	if err != nil {
		logger.Error(fmt.Sprintf("Error reading config: %v", err))
		os.Exit(1)
	}
	if err := cfg.Validate(); err != nil {
		logger.Error(fmt.Sprintf("Error validating config: %v", err))
		os.Exit(1)
	}
	defer cfg.Save()

	if err := config.SetupLogging(cfg); err != nil {
		logger.Error(fmt.Sprintf("Error setting up logging: %v", err))
		os.Exit(1)
	}
	defer config.CloseLogging()

	if err := network.SocketListen(cfg.Port); err != nil {
		logger.Error(fmt.Sprintf("Error starting socket listener: %v", err))
		os.Exit(1)
	}
	defer network.SocketClose()

	closing := make(chan bool)
	go handleMessages(closing)

	terminate := make(chan os.Signal, 1)
	signal.Notify(terminate, os.Interrupt)
	<-terminate

	logger.Info("Shutting down...")
	// send closing signal then wait for confirmation before exiting
	closing <- true
	<-closing
}

func handleMessages(closing chan bool) {
	logger := config.GetLogger()
	logger.Info("Listening for UDP packets.")
	for {
		select {
		case <-closing:
			logger.Info("Stopping UDP listener")
			closing <- true
			return
		default:
		}

		session, player, data := network.SocketRecv()
		if session == nil || player == nil || data == nil {
			logger.Debug("Received nil session, player, or data. Skipping.")
			continue
		}

		// Forward the data to the other client
		_, otherPlayer := network.GetClientFunc(*session, func(c network.SocketClient) bool {
			return c.PlayerType != player.PlayerType
		})
		if otherPlayer == nil {
			logger.Error(fmt.Sprintf("No other player to forward data to for session %s", session.ID))
			continue
		}
		network.SocketSend(otherPlayer, data)
	}
}
