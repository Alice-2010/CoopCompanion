package network

import (
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"os"
	"time"
)

type SocketDataType string
type SocketData struct {
	Type SocketDataType `json:"type"`
	Data any            `json:"data,omitempty"`
}

const (
	SERVER_ADDRESS = "0.0.0.0"
	SERVER_PORT    = 7243

	SOCKET_DATA_TYPE_CONNECT    SocketDataType = "connect"
	SOCKET_DATA_TYPE_DISCONNECT SocketDataType = "disconnect"
)

var (
	conn    *net.UDPConn
	clients = struct {
		gameHook    *net.UDPAddr
		otherPlayer *net.UDPAddr
	}{}
)

func SocketListen() {
	c, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP(SERVER_ADDRESS),
		Port: SERVER_PORT,
	})
	if err != nil {
		panic(err)
	}
	conn = c
}

func SocketClose() {
	if conn != nil {
		clients.gameHook = nil
		clients.otherPlayer = nil
		conn.Close()
	}
}

func SocketRecv(closing chan bool) {
	fmt.Printf("Listening for UDP packets on %v\n", conn.LocalAddr())
	for {
		select {
		case <-closing:
			fmt.Println("Stopping UDP listener")
			closing <- true
			return
		default:
		}

		if conn == nil {
			fmt.Println("Socket is not initialized")
			break
		}
		b := make([]byte, 1024)

		// Set a read timeout to avoid blocking indefinitely
		conn.SetReadDeadline(time.Now().Add(3 * time.Second))
		n, addr, err := conn.ReadFromUDP(b)
		if err != nil {
			if errors.Is(err, net.ErrClosed) {
				fmt.Println("Socket is closed")
				return
			} else if errors.Is(err, os.ErrDeadlineExceeded) {
				// Timeout occurred, continue to the next iteration to check for closing signal
				continue
			}
			fmt.Printf("Error receiving data: %v\n", err)
			continue
		}

		// Determine which client sent the data
		// gameHook socket is expected to be from localhost, while otherPlayer socket is expected to be from a different IP
		var client *net.UDPAddr
		var sendTo *net.UDPAddr
		if addr.IP.IsLoopback() {
			client = clients.gameHook
			sendTo = clients.otherPlayer
		} else {
			client = clients.otherPlayer
			sendTo = clients.gameHook
		}

		if client != nil && client.String() != addr.String() {
			fmt.Printf("Ignoring data from unknown client: %s\n", addr)
			continue
		}

		data := new(SocketData)
		if err := json.Unmarshal(b[:n], data); err != nil {
			fmt.Println("Error unmarshaling data:", err)
			continue
		}

		switch data.Type {
		case SOCKET_DATA_TYPE_CONNECT:
			fmt.Println("Client connected")
			client = addr
			continue
		case SOCKET_DATA_TYPE_DISCONNECT:
			fmt.Println("Client disconnected")
			client = nil
			continue
		}
		fmt.Printf("Received %d bytes from %s: %v\n", n, addr, data)
		if sendTo == nil {
			fmt.Println("No destination specified, ignoring data")
			continue
		}
		SocketSend(sendTo, data)
	}
}

func SocketSend(sendTo *net.UDPAddr, data *SocketData) {
	if conn == nil {
		fmt.Println("Socket is not initialized")
		return
	}
	if sendTo == nil {
		fmt.Println("No destination specified")
		return
	}

	buffer, err := json.Marshal(data)
	if err != nil {
		fmt.Println("Error marshaling data:", err)
		return
	}
	if _, err := conn.WriteToUDP(buffer, sendTo); err != nil {
		fmt.Println("Error sending data:", err)
		return
	}
	fmt.Printf("Sent %d bytes to %s: %v\n", len(buffer), sendTo, data)
}
