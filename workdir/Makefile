all: cppdemo cdemo

clean:
	rm -f cppdemo cdemo

cdemo: ../src/*.c
	clang -O3 ../src/*.c -I./include -lglfw -lGLEW -framework OpenCL -lfreeimage -o cdemo -framework OpenGL

cppdemo: ../src/*.cpp
	clang++ -O3 ../src/*.cpp -lglfw -lGLEW -framework OpenCL -lfreeimage -o cppdemo -framework OpenGL
