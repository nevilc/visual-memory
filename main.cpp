#include <string>
#include <iostream>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif//WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
	#define NOMINMAX
#endif//NOMINMAX
#include <Windows.h>
#include <WindowsX.h>
#include <tlhelp32.h>

#define SFML_STATIC
#include <SFML/Graphics.hpp>

struct color_format {
	int bytes_per_pixel;
	int red_byte;
	int blue_byte;
	int green_byte;
	int alpha_byte;
};

color_format Mono8 = {1, 0, 0, 0, -255};
color_format RGBA32 = {4, 0, 1, 2, 3};
color_format RGB24 = {3, 0, 1, 2, -255};
color_format ABGR32 = {4, 3, 2, 1, 0};
color_format BGR24 = {3, 2, 1, 0, -255};

struct display_settings {
	color_format* pixel_format;
	int width;
	int byte_offset;
	int pixel_offset;

	char* memory_block;
};

int min_block_size = 1024 * 1024 * 1;

DWORD find_process_by_name(std::string process_name) {
	HANDLE system_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process;
	process.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(system_snapshot, &process) == TRUE) {
		// Iterate through all the processes
		while (Process32Next(system_snapshot, &process) == TRUE) {
			// Find the target process by name
			if (_stricmp(process.szExeFile, process_name.c_str()) == 0) {
				return process.th32ProcessID;
			}
		}
	}
	return 0;
}

HANDLE open_process(DWORD pid) {
	// Open the process with permission to read memory and process info
	HANDLE process_handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);

	if (process_handle == NULL) {
		fprintf(stderr, "Could not open process id %i\n", pid);
		exit(EXIT_FAILURE);
	}

	return process_handle;
}

void close_process(HANDLE process_handle) {
	CloseHandle(process_handle);
}

MEMORY_BASIC_INFORMATION find_next_memory_block(HANDLE process_handle, void* address) {
	MEMORY_BASIC_INFORMATION mbi;
	
	VirtualQueryEx(process_handle, address, &mbi, sizeof(mbi));
	do {
		address = (char*)mbi.BaseAddress + mbi.RegionSize;
		while (!VirtualQueryEx(process_handle, address, &mbi, sizeof(mbi))) {
			address = 0x00000000;
		}
	} while (mbi.State == MEM_FREE || mbi.State == MEM_RESERVE || mbi.RegionSize < min_block_size);

	return mbi;
}

void convert_memory_to_texture(HANDLE process, MEMORY_BASIC_INFORMATION mbi, display_settings ds, sf::Texture& texture) {
	SIZE_T bytes_read;
	SIZE_T expected_bytes = mbi.RegionSize - (ds.byte_offset + ds.pixel_offset * ds.pixel_format->bytes_per_pixel);
	unsigned char* image_data = new unsigned char[expected_bytes * 4 / ds.pixel_format->bytes_per_pixel];
	
	ReadProcessMemory(process, (LPVOID)(ds.memory_block + ds.byte_offset + ds.pixel_offset * ds.pixel_format->bytes_per_pixel), (LPVOID)image_data, expected_bytes, &bytes_read);
	int width = ds.width;
	if (bytes_read != expected_bytes) {
		std::cerr << "Error reading process memory\n";
		return;
	}
	int height = bytes_read / (ds.pixel_format->bytes_per_pixel * ds.width);

	if (ds.pixel_format != &RGBA32) {
		for (int i = width * height - 1; i >= 0; --i) {
			unsigned char r;
			unsigned char g;
			unsigned char b;
			unsigned char a;
			if (ds.pixel_format->red_byte >= 0) {
				r = image_data[i*ds.pixel_format->bytes_per_pixel + ds.pixel_format->red_byte];
			} else {
				r = -ds.pixel_format->red_byte;
			}
			if (ds.pixel_format->blue_byte >= 0) {
				g = image_data[i*ds.pixel_format->bytes_per_pixel + ds.pixel_format->blue_byte];
			} else {
				g = -ds.pixel_format->blue_byte;
			}
			if (ds.pixel_format->green_byte >= 0) {
				b = image_data[i*ds.pixel_format->bytes_per_pixel + ds.pixel_format->green_byte];
			} else {
				b = -ds.pixel_format->green_byte;
			}
			if (ds.pixel_format->alpha_byte >= 0) {
				a = image_data[i*ds.pixel_format->bytes_per_pixel + ds.pixel_format->alpha_byte];
			} else {
				a = -ds.pixel_format->alpha_byte;
			}
			image_data[i*4] = r;
			image_data[i*4+1] = g;
			image_data[i*4+2] = b;
			image_data[i*4+3] = a;
		}
	}
	
	sf::Image image;
	image.create(ds.width, height, image_data);

	texture.loadFromImage(image);

	delete[] image_data;
}

int main(int argc, char** argv) {
	sf::RenderWindow main_window(sf::VideoMode(1200, 960), "Memory Visualizer", sf::Style::Close | sf::Style::Titlebar);
	main_window.setFramerateLimit(30);

	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " process\n";
		return EXIT_SUCCESS;
	}

	DWORD pid;

	bool is_pid = true;
	for (int i = 0; argv[1][i] != '\0'; ++i) {
		if ((argv[1][i] < '0' || argv[1][i] > '9') && argv[1][i] != ' ') {
			is_pid = false;
			break;
		}
	}

	if (is_pid) {
		pid = atoi(argv[1]);
	} else {
		std::string process_name(argv[1]);
		pid = find_process_by_name(process_name);
		std::cout << "pid = " << pid << '\n';
	}

	display_settings ds;
	ds.pixel_offset = 0;
	//ds.byte_offset = 17*4;
	ds.byte_offset = 0;
	ds.width = 800;
	ds.pixel_format = &RGB24;

	if (argc >= 3) {
		if (strcmp("RGBA32", argv[2]) == 0) {
			ds.pixel_format = &RGBA32;
		} else if (strcmp("Mono8", argv[2]) == 0) {
			ds.pixel_format = &Mono8;
		} else if (strcmp("RGB24", argv[2]) == 0) {
			ds.pixel_format = &RGB24;
		} else if (strcmp("BGR24", argv[2]) == 0) {
			ds.pixel_format = &BGR24;
		} else if (strcmp("ABGR32", argv[2]) == 0) {
			ds.pixel_format = &ABGR32;
		} else {
			std::cerr << "Unknown color format " << argv[2] << '\n';
		}
	}

	if (argc >= 4) {
		min_block_size = atoi(argv[3]);
	}

	HANDLE process = open_process(pid);
	MEMORY_BASIC_INFORMATION mbi = find_next_memory_block(process, 0x00000000);
	ds.memory_block = (char*)mbi.BaseAddress;

	sf::Texture texture;
	sf::Sprite sprite;
	sf::View view = main_window.getDefaultView();
	main_window.setView(view);
	convert_memory_to_texture(process, mbi, ds, texture);
	sprite.setTexture(texture);
	sprite.setPosition(0.0f, 0.0f);

	while (main_window.isOpen()) {
		sf::Event event;

		bool dirty_position = false;
		bool dirty_texture = false;
		bool dirty_pixel_offset = false;

		while (main_window.pollEvent(event)) {
			switch (event.type) {
			case sf::Event::Closed:
                main_window.close();
				break;
			case sf::Event::KeyPressed:
				switch (event.key.code) {
				case sf::Keyboard::Escape:
					main_window.close();
					break;
				case sf::Keyboard::Return:
					mbi = find_next_memory_block(process, (void*)ds.memory_block);
					ds.memory_block = (char*)mbi.BaseAddress;
					std::cout << (void*)ds.memory_block << " (" << mbi.RegionSize << " bytes)\n";
					ds.pixel_offset = 0;
					dirty_texture = true;
					sprite.setPosition(0.0f, 0.0f);
					break;
				case sf::Keyboard::Num1:
					ds.byte_offset = 0;
					dirty_texture = true;
					break;
				case sf::Keyboard::Num2:
					ds.byte_offset = 1;
					dirty_texture = true;
					break;
				case sf::Keyboard::Num3:
					ds.byte_offset = 2;
					dirty_texture = true;
					break;
				case sf::Keyboard::Num4:
					ds.byte_offset = 3;
					dirty_texture = true;
					break;
				case sf::Keyboard::F1:
					std::cout << '(' << texture.getSize().x << " x " << texture.getSize().y << ")\n";
					std::cout << (void*)ds.memory_block << '\n';
					{
						const size_t num = 68;
						auto* image_data = new unsigned char[num];
						ReadProcessMemory(process, (LPVOID)(ds.memory_block), (LPVOID)image_data, sizeof(*image_data) * num, NULL);
						for (int i = 0; i < num; ++i) {
							std::cout << (unsigned int)image_data[i] << "\n";
						}
						delete[] image_data;
					}
					break;
				case sf::Keyboard::F12:
					std::cout << "Image saved as " << "out.png" << '\n';
					texture.copyToImage().saveToFile("out.png");
					break;
				}
				break;
			case sf::Event::Resized:
				
				break;
			}
		}

		bool shifted = (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift));
		bool alted = (sf::Keyboard::isKeyPressed(sf::Keyboard::LAlt) || sf::Keyboard::isKeyPressed(sf::Keyboard::RAlt));

		int magnitude = 4;
		if (shifted) {
			magnitude *= 2;
		}
		if (alted) {
			magnitude /= 4;
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Comma)) {
			ds.pixel_offset -= magnitude;
			dirty_pixel_offset = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Period)) {
			ds.pixel_offset += magnitude;
			dirty_pixel_offset = true;
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
			sprite.setPosition(sprite.getPosition().x - magnitude, sprite.getPosition().y);
			dirty_position = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
			sprite.setPosition(sprite.getPosition().x + magnitude, sprite.getPosition().y);
			dirty_position = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
			sprite.setPosition(sprite.getPosition().x, sprite.getPosition().y - magnitude);
			dirty_position = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
			sprite.setPosition(sprite.getPosition().x, sprite.getPosition().y + magnitude);
			dirty_position = true;
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::LBracket)) {
			ds.width -= magnitude;
			dirty_texture = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::RBracket)) {
			ds.width += magnitude;
			dirty_texture = true;
		}

		if (dirty_pixel_offset) {
			ds.pixel_offset %= texture.getSize().x;
			dirty_texture = true;
		}

		if (dirty_position) {
			sprite.setPosition(
				std::min(
					0.0f, 
					std::max(
						float(main_window.getSize().x) - sprite.getTextureRect().width, 
						sprite.getPosition().x
					)
				), 
				std::min(
					0.0f, 
					std::max(
						float(main_window.getSize().y) - sprite.getTextureRect().height, 
						sprite.getPosition().y
					)
				)
			);
		}

		if (dirty_texture) {
			convert_memory_to_texture(process, mbi, ds, texture);
			sprite.setTexture(texture, true);
		}

		main_window.clear();
		main_window.draw(sprite);

		main_window.display();
	}

	return EXIT_SUCCESS;
}
