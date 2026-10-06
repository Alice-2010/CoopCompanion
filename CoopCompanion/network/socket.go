package network

import (
	"alice-coop-companion/config"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"os"
	"strings"
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
	Version   GameVersionHash
	Clients   []SocketClient
}

const (
	serverAddress string = "0.0.0.0"

	playerTypeHost PlayerType = "host"
	playerTypeJoin PlayerType = "join"
)

var (
	conn     *net.UDPConn
	sessions = map[string]Session{}
)

func createSessionID() string {
	chars := "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	var id strings.Builder
	for i := range 14 {
		if i == 4 || i == 10 {
			id.WriteString("-")
		}
		id.WriteString(string(chars[i%len(chars)]))
	}
	return id.String()
}

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

func GetClientFunc(s Session, f func(SocketClient) bool) (int, *SocketClient) {
	for index, client := range s.Clients {
		if f(client) {
			return index, &client
		}
	}
	return -1, nil
}

func SocketListen(port int) error {
	c, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP(serverAddress),
		Port: port,
	})
	if err != nil {
		return err
	}
	conn = c
	return nil
}

func SocketClose() {
	if conn != nil {
		conn.Close()
	}
}

func SocketRecv() (*Session, *SocketClient, *SocketData) {
	logger := config.GetLogger()
	if conn == nil {
		logger.Error("Socket is not initialized")
		return nil, nil, nil
	}
	b := make([]byte, 1024)

	// Set a read timeout to avoid blocking indefinitely
	conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	n, addr, err := conn.ReadFromUDP(b)
	if err != nil {
		if errors.Is(err, net.ErrClosed) {
			logger.Error("Socket is closed")
			return nil, nil, nil
		} else if errors.Is(err, os.ErrDeadlineExceeded) {
			// Timeout occurred, continue to the next iteration to check for closing signal
			return nil, nil, nil
		}
		logger.Error(fmt.Sprintf("Error receiving data: %v", err))
		return nil, nil, nil
	}

	data := new(SocketData)
	if err := json.Unmarshal(b[:n], data); err != nil {
		logger.Error(fmt.Sprintf("Error unmarshaling data: %v", err))
		return nil, nil, nil
	}
	if err := data.Validate(); err != nil {
		logger.Error(fmt.Sprintf("Invalid data received: %v", err))
		return nil, nil, nil
	}

	// Determine which client sent the data
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
	if !isAlreadyInSession && (data.Type == socketDataTypeJoin || data.Type == socketDataTypeHost) {
		switch data.Type {
		case socketDataTypeJoin:
			logger.Info(fmt.Sprintf("Player joining session: %s", addr))
			session, ok := sessions[data.SessionID]
			if !ok {
				logger.Error(fmt.Sprintf("Session %s does not exist", data.SessionID))
				return nil, nil, nil
			}
			if len(session.Clients) >= 2 {
				logger.Error(fmt.Sprintf("Session %s is full", data.SessionID))
				SocketSend(&SocketClient{addr: addr}, &SocketData{Type: socketDataTypeJoinDeclined})
				return nil, nil, nil
			}
			session.Clients = append(session.Clients, SocketClient{addr: addr, ConnectedAt: time.Now(), PlayerType: playerTypeJoin})
			sessions[data.SessionID] = session
		case socketDataTypeHost:
			logger.Info(fmt.Sprintf("Creating new session for host: %s", addr))
			sessionID := createSessionID()
			sessions[sessionID] = Session{
				ID:        sessionID,
				CreatedAt: time.Now(),
				Version:   data.Version,
				Clients:   []SocketClient{{addr: addr, ConnectedAt: time.Now(), PlayerType: playerTypeHost}},
			}
		}
	} else {
		logger.Error(fmt.Sprintf("Ignoring data from already connected host: %s", addr))
		return nil, nil, nil
	}

	sID, session, playerIndex, player := GetClientSessionFunc(
		func(s Session) bool { return true }, // Match any session
		func(c SocketClient) bool { return c.addr.String() == addr.String() },
	)

	if sID == "" || session == nil || playerIndex == -1 || player == nil {
		logger.Error(fmt.Sprintf("Ignoring data from unknown client: %s", addr))
		return nil, nil, nil
	}

	switch data.Type {
	case socketDataTypeLeave:
		logger.Info(fmt.Sprintf("Client %s left the game", addr))
		if len(session.Clients) == 1 {
			delete(sessions, sID)
			logger.Info(fmt.Sprintf("Last client left, deleting session %s", sID))
		} else if player.PlayerType == playerTypeHost {
			session.Clients = append(session.Clients[:playerIndex], session.Clients[playerIndex+1:]...)
			session.Clients[0].PlayerType = playerTypeHost
			logger.Info("Host left, player 2 has become host")
		} else {
			session.Clients = append(session.Clients[:playerIndex], session.Clients[playerIndex+1:]...)
			logger.Info(fmt.Sprintf("Player %s left, host remains", addr))
		}
	}
	logger.Debug(fmt.Sprintf("Received %d bytes from %s: %v", n, addr, data))
	return session, player, data
}

func SocketSend(client *SocketClient, data *SocketData) {
	logger := config.GetLogger()
	if conn == nil {
		logger.Error("Socket is not initialized")
		return
	}
	if client == nil || client.addr == nil {
		logger.Error("No destination specified")
		return
	}

	buffer, err := json.Marshal(data)
	if err != nil {
		logger.Error(fmt.Sprintf("Error marshaling data: %v", err))
		return
	}
	if _, err := conn.WriteToUDP(buffer, client.addr); err != nil {
		logger.Error(fmt.Sprintf("Error sending data: %v", err))
		return
	}
	logger.Debug(fmt.Sprintf("Sent %d bytes to %s: %v", len(buffer), client.addr, data))
}
