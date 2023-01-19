TARGET_1 = producer 
TARGET_2 = consumer
TARGET_3 = initializer
OBJECTS_1 = LamportQueue.o producer.o 
OBJECTS_2 = LamportQueue.o consumer.o
OBJECTS_3 = LamportQueue.o initializer.o
CXX = g++
CXXFLAGS = -std=c++20 -O2

all: $(TARGET_1) $(TARGET_2) $(TARGET_3)

$(TARGET_1): $(OBJECTS_1)
	$(CXX) $(CXXFLAGS) $(OBJECTS_1) -o $(TARGET_1)

$(TARGET_2): $(OBJECTS_2)
	$(CXX) $(CXXFLAGS) $(OBJECTS_2) -o $(TARGET_2)

$(TARGET_3): $(OBJECTS_3)
	$(CXX) $(CXXFLAGS) $(OBJECTS_3) -o $(TARGET_3)

%.o: ../../queue/Cpp/%.cpp ../../queue/Cpp/LamportQueue.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

initializer.o: initializer.cpp ../../queue/Cpp/LamportQueue.hpp
	$(CXX) $(CXXFLAGS) -c initializer.cpp

clean:
	rm -f $(OBJECTS_1) $(TARGET_1) $(OBJECTS_2) $(TARGET_2) $(OBJECTS_3) $(TARGET_3)
