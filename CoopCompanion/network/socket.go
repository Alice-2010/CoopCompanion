package network

import (
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"os"
	"time"
)

type PlayerType string
type SocketClient struct {
	PlayerType  PlayerType
	ConnectedAt time.Time
	addr        *net.UDPAddr
}
type Session struct {
	ID        string
	CreatedAt time.Time
	Clients   []SocketClient
}

const (
	serverAddress string = "0.0.0.0"
	serverPort    int    = 7243

	playerTypeHost PlayerType = "host"
	playerTypeJoin PlayerType = "join"
)

var (
	conn     *net.UDPConn
	sessions = map[string]Session{}
)

func GetClientSessionFunc(s func(Session) bool, c func(SocketClient) bool) (string, *Session, int, *SocketClient) {
	for sID, session := range sessions {
		if s(session) {
			for index, client := range session.Clients {
				if c(client) {
					return sID, &session, index, &client
				}
			}
		}
	}
	return "", nil, -1, nil
}

func GetSessionFunc(s func(Session) bool) (string, *Session) {
	for sID, session := range sessions {
		if s(session) {
			return sID, &session
		}
	}
	return "", nil
}

func GetClientFunc(s Session, f func(SocketClient) bool) (int, *SocketClient) {
	for index, client := range s.Clients {
		if f(client) {
			return index, &client
		}
	}
	return -1, nil
}

func SocketListen() {
	c, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP(serverAddress),
		Port: serverPort,
	})
	if err != nil {
		panic(err)
	}
	conn = c
}

func SocketClose() {
	if conn != nil {
		conn.Close()
	}
}

func SocketRecv() (*Session, *SocketClient, *SocketData) {
	if conn == nil {
		fmt.Println("Socket is not initialized")
		return nil, nil, nil
	}
	b := make([]byte, 1024)

	// Set a read timeout to avoid blocking indefinitely
	conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	n, addr, err := conn.ReadFromUDP(b)
	if err != nil {
		if errors.Is(err, net.ErrClosed) {
			fmt.Println("Socket is closed")
			return nil, nil, nil
		} else if errors.Is(err, os.ErrDeadlineExceeded) {
			// Timeout occurred, continue to the next iteration to check for closing signal
			return nil, nil, nil
		}
		fmt.Printf("Error receiving data: %v\n", err)
		return nil, nil, nil
	}

	data := new(SocketData)
	if err := json.Unmarshal(b[:n], data); err != nil {
		fmt.Printf("Error unmarshaling data: %v\n", err)
		return nil, nil, nil
	}
	if err := data.Validate(); err != nil {
		fmt.Printf("Invalid data received: %v\n", err)
		return nil, nil, nil
	}

	// Determine which client sent the data
	if data.Type == socketDataTypeHost {
		isAlreadyInSession := false
		for _, session := range sessions {
			for _, client := range session.Clients {
				if client.addr.String() == addr.String() {
					isAlreadyInSession = true
					break
				}
			}
			if isAlreadyInSession {
				break
			}
		}
		if !isAlreadyInSession {
			sessionID := "abcdefghik" // TODO: Generate a unique session ID
			sessions[sessionID] = Session{
				ID:        sessionID,
				CreatedAt: time.Now(),
				Clients:   []SocketClient{{addr: addr, ConnectedAt: time.Now(), PlayerType: playerTypeHost}},
			}
		} else {
			fmt.Printf("Ignoring data from already connected host: %s\n", addr)
			return nil, nil, nil
		}
	}

	sID, session, playerIndex, player := GetClientSessionFunc(
		func(s Session) bool { return true }, // Match any session
		func(c SocketClient) bool { return c.addr.String() == addr.String() },
	)

	if sID == "" || session == nil || playerIndex == -1 || player == nil {
		fmt.Printf("Ignoring data from unknown client: %s\n", addr)
		return nil, nil, nil
	}

	switch data.Type {
	case socketDataTypeHost:
		fmt.Printf("Host connected from %s\n", addr)
	case socketDataTypeJoin:
		if len(session.Clients) == 2 {
			fmt.Printf("Player has already joined\n")
			SocketSend(player, &SocketData{Type: socketDataTypeJoinDeclined})
			return nil, nil, nil
		}
		session.Clients = append(session.Clients, SocketClient{addr: addr, ConnectedAt: time.Now(), PlayerType: playerTypeJoin})
		fmt.Printf("Player connected from %s\n", addr)
	case socketDataTypeLeave:
		fmt.Printf("Client %s left the game\n", addr)
		if len(session.Clients) == 1 {
			delete(sessions, sID)
			fmt.Printf("Last client left, deleting session %s\n", sID)
		} else if player.PlayerType == playerTypeHost {
			session.Clients = append(session.Clients[:playerIndex], session.Clients[playerIndex+1:]...)
			session.Clients[0].PlayerType = playerTypeHost
			fmt.Println("Host left, player 2 has become host")
		} else {
			session.Clients = append(session.Clients[:playerIndex], session.Clients[playerIndex+1:]...)
			fmt.Printf("Player %s left, host remains\n", addr)
		}
	}
	fmt.Printf("Received %d bytes from %s: %v\n", n, addr, data)
	return session, player, data
}

func SocketSend(client *SocketClient, data *SocketData) {
	if conn == nil {
		fmt.Println("Socket is not initialized")
		return
	}
	if client == nil || client.addr == nil {
		fmt.Println("No destination specified")
		return
	}

	buffer, err := json.Marshal(data)
	if err != nil {
		fmt.Printf("Error marshaling data: %v\n", err)
		return
	}
	if _, err := conn.WriteToUDP(buffer, client.addr); err != nil {
		fmt.Printf("Error sending data: %v\n", err)
		return
	}
	fmt.Printf("Sent %d bytes to %s: %v\n", len(buffer), client.addr, data)
}
