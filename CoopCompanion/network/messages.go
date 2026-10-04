package network

import (
	"errors"
	"reflect"
	"slices"
)

type SocketDataType string
type SocketData struct {
	Type      SocketDataType `json:"type"`
	SessionID string         `json:"session_id,omitempty"`
	Data      any            `json:"data,omitempty"`
}

const (
	socketDataTypeHost         SocketDataType = (SocketDataType)(playerTypeHost)
	socketDataTypeJoin         SocketDataType = (SocketDataType)(playerTypeJoin)
	socketDataTypeJoinDeclined SocketDataType = "join_declined"
	socketDataTypeLeave        SocketDataType = "leave"
)

var (
	socketDataTypes = []SocketDataType{
		socketDataTypeHost,
		socketDataTypeJoin,
		socketDataTypeJoinDeclined,
		socketDataTypeLeave,
	}
	socketDataTypeToDataType = map[SocketDataType]any{}
)

func (s *SocketData) Validate() error {
	if !slices.Contains(socketDataTypes, s.Type) {
		return errors.New("invalid socket data type")
	}
	if s.Type != socketDataTypeHost {
		if s.SessionID == "" {
			return errors.New("session ID is required")
		}
		if _, ok := sessions[s.SessionID]; !ok {
			return errors.New("session does not exist")
		}
	}
	if s.Data != nil {
		dataType, ok := socketDataTypeToDataType[s.Type]
		if !ok || reflect.TypeOf(s.Data) != reflect.TypeOf(dataType) {
			return errors.New("data type mismatch")
		}
	}
	return nil
}
