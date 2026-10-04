package config

import (
	"encoding/json"
	"fmt"
	"log/slog"
	"os"
	"slices"
)

type Config struct {
	// The port to listen on for incoming connections
	Port int `json:"port"`
	// The log level to use for logging
	LogLevel slog.Level `json:"log_level"`
	// The file path to save the logs to, if file logging is enabled
	LogFile string `json:"log_file,omitempty"`
	// The logging methods to use for logging
	LoggingMethods []LoggingMethod `json:"logging_methods,omitempty"`
}

const (
	defaultPort     int        = 7243
	defaultLogLevel slog.Level = slog.LevelInfo
	defaultLogFile  string     = "coop_companion.log"
	configFile      string     = "config.json"
)

var (
	defaultLoggingMethods = []LoggingMethod{LoggingMethodConsole}
)

func ReadConfig() (*Config, error) {
	// Read the config file
	configFile, err := os.Open(configFile)
	if err != nil {
		if os.IsNotExist(err) {
			return &Config{
				Port:           defaultPort,
				LogLevel:       defaultLogLevel,
				LogFile:        defaultLogFile,
				LoggingMethods: defaultLoggingMethods,
			}, nil
		}
		return nil, fmt.Errorf("failed to read config file: %w", err)
	}
	defer configFile.Close()

	config := &Config{}
	// Decode the config file into the Config struct
	err = json.NewDecoder(configFile).Decode(config)
	if err != nil {
		return nil, fmt.Errorf("failed to decode config file: %w", err)
	}
	return config, nil
}

func (c *Config) Validate() error {
	logger := GetLogger()
	// Port
	if c.Port < 1 || c.Port > 65535 {
		logger.Warn(fmt.Sprintf("Invalid port: %d, using default: %d", c.Port, defaultPort))
		c.Port = defaultPort
	}

	// Log level
	if !slices.Contains(logLevels, c.LogLevel) {
		logger.Warn(fmt.Sprintf("Invalid log level: %s, using default: %s", c.LogLevel, defaultLogLevel))
		c.LogLevel = defaultLogLevel
	}

	// Log methods
	for idx, method := range c.LoggingMethods {
		if !slices.Contains(loggingMethods, method) {
			logger.Warn(fmt.Sprintf("Invalid logging method: %s, removing from list", method))
			c.LoggingMethods = append(c.LoggingMethods[:idx], c.LoggingMethods[idx+1:]...)
		}
		if slices.Contains(c.LoggingMethods[idx+1:], method) {
			logger.Warn(fmt.Sprintf("Multiple %s logging method found, removing one", method))
			c.LoggingMethods = append(c.LoggingMethods[:idx], c.LoggingMethods[idx+1:]...)
		}
	}
	if len(c.LoggingMethods) == 0 {
		logger.Warn(fmt.Sprintf("No logging methods specified, using default: %v", defaultLoggingMethods))
		c.LoggingMethods = defaultLoggingMethods
	}

	// Log file
	if slices.Contains(c.LoggingMethods, LoggingMethodFile) && c.LogFile == "" {
		logger.Warn(fmt.Sprintf("File logging enabled but no log file specified, using default: %s", defaultLogFile))
		c.LogFile = defaultLogFile
	}

	return nil
}

func (c *Config) Save() error {
	file, err := os.Open(configFile)
	if err != nil {
		if os.IsNotExist(err) {
			file, err = os.Create(configFile)
		}
		if err != nil {
			return fmt.Errorf("failed to create config file: %w", err)
		}
	}
	defer file.Close()

	return json.NewEncoder(file).Encode(c)
}
