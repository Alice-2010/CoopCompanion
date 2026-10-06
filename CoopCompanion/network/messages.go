package network

import (
	"errors"
	"reflect"
	"slices"
)

type GameVersionHash string
type SocketDataType string
type SocketDataData interface {
	Validate() error
}
type SocketData struct {
	Type      SocketDataType  `json:"type"`
	Version   GameVersionHash `json:"version"`
	SessionID string          `json:"session_id,omitempty"`
	Data      SocketDataData  `json:"data,omitempty"`
}

const (
	socketDataTypeHost         SocketDataType = (SocketDataType)(playerTypeHost)
	socketDataTypeJoin         SocketDataType = (SocketDataType)(playerTypeJoin)
	socketDataTypeJoinDeclined SocketDataType = "join_declined"
	socketDataTypeLeave        SocketDataType = "leave"

	// SHA256 hash of the game version, used to ensure that players are running the same version of the game
	gameVersionHashSteam     GameVersionHash = "73B5DC089D2A9678A2BD519647986F23E5C705ADDDA4981029774CA179625B68"
	gameVersionHashPolishRom GameVersionHash = "2578F6BCEC330CC3B4C055FCFEEA52258FB6D3D216787B1C547C4BB56CD628F6"
)

var (
	socketDataTypes = []SocketDataType{
		socketDataTypeHost,
		socketDataTypeJoin,
		socketDataTypeJoinDeclined,
		socketDataTypeLeave,
	}
	socketDataTypeToDataType = map[SocketDataType]SocketDataData{}
	gameVersionHashes        = []GameVersionHash{
		gameVersionHashSteam,
		gameVersionHashPolishRom,
	}
)

func (g GameVersionHash) String() string {
	switch g {
	case gameVersionHashSteam:
		return "Steam"
	case gameVersionHashPolishRom:
		return "Polish DVDROM"
	default:
		return "Unknown"
	}
}

func (s *SocketData) Validate() error {
	if !slices.Contains(socketDataTypes, s.Type) {
		return errors.New("invalid socket data type")
	}
	if s.Type != socketDataTypeHost {
		if s.SessionID == "" {
			return errors.New("session ID is required")
		}
		session, ok := sessions[s.SessionID]
		if !ok {
			return errors.New("session does not exist")
		}
		if s.Version != session.Version {
			return errors.New("version mismatch")
		}
	} else if !slices.Contains(gameVersionHashes, s.Version) {
		return errors.New("invalid game version")
	}
	if s.Data != nil {
		dataType, ok := socketDataTypeToDataType[s.Type]
		if !ok || reflect.TypeOf(s.Data) != reflect.TypeOf(dataType) {
			return errors.New("data type mismatch")
		}
		if err := s.Data.Validate(); err != nil {
			return err
		}
	}
	return nil
}
