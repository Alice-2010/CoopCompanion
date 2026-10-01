.PHONY: build
build:
	@rm -rf bin
	@mkdir bin
	@GOOS=windows go build -o bin/CoopCompanion.exe -ldflags "-s -w" -trimpath -buildmode=exe -tags=windows main.go
