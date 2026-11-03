CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror
TEST_BINARY := ../../build/tests/recorder-stop-test

.PHONY: all clean

all: $(TEST_BINARY)
	python3 -c 'import subprocess; subprocess.run(["$(TEST_BINARY)"], check=True, timeout=5)'

$(TEST_BINARY): RecorderStopTest.cpp stubs/Audio.h stubs/mtAudioEngine.h stubs/polyassert.h \
	../source/Recorder.cpp ../source/Recorder.h
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -Istubs -I../source RecorderStopTest.cpp -o $@

clean:
	rm -f $(TEST_BINARY)
