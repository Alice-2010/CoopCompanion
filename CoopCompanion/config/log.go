package config

import (
	"log/slog"
	"os"
)

type LoggingMethod string

const (
	LoggingMethodConsole LoggingMethod = "console"
	LoggingMethodFile    LoggingMethod = "file"
)

var (
	logger    *slog.Logger = nil
	logLevels              = []slog.Level{
		slog.LevelDebug,
		slog.LevelInfo,
		slog.LevelWarn,
		slog.LevelError,
	}
	loggingMethods = []LoggingMethod{
		LoggingMethodConsole,
		LoggingMethodFile,
	}
	logFile *os.File = nil
)

func SetupLogging(cfg *Config) error {
	handlers := []slog.Handler{}
	opts := &slog.HandlerOptions{
		AddSource: false,
		Level:     cfg.LogLevel,
	}
	for _, method := range cfg.LoggingMethods {
		switch method {
		case LoggingMethodConsole:
			handlers = append(handlers, slog.NewTextHandler(os.Stdout, opts))
		case LoggingMethodFile:
			file, err := os.OpenFile(cfg.LogFile, os.O_CREATE|os.O_WRONLY, 0644)
			if err != nil {
				CloseLogging()
				return err
			}
			logFile = file
			handlers = append(handlers, slog.NewTextHandler(file, opts))
		}
	}
	logger = slog.New(slog.NewMultiHandler(handlers...))
	slog.SetDefault(logger)
	slog.SetLogLoggerLevel(cfg.LogLevel)
	return nil
}

func CloseLogging() {
	if logFile != nil {
		logFile.Close()
	}
}

func GetLogger() *slog.Logger {
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{
			AddSource: false,
			Level:     defaultLogLevel,
		}))
	}
	return logger
}
