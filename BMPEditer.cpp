#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

#pragma pack(push,1)
struct BMPFileHeader {
	uint16_t file_type{ 0x4D42 };
	uint32_t file_size{};
	uint16_t reserved1{ 0 };
	uint16_t reserved2{ 0 };
	uint32_t offset_data{ 0 };
};

struct BMPInfoHeader {
	uint32_t size{ 0 };
	int32_t width{ 0 };
	int32_t height{ 0 };
	uint16_t planes{ 1 };
	uint16_t bit_count{ 0 };
	uint32_t compression{ 0 };
	uint32_t size_image{ 0 };
	int32_t x_pixels_per_meter{ 0 };
	int32_t y_pixels_per_meter{ 0 };
	uint32_t colors_used{ 0 };
};

struct BMPColorHeader {
	uint32_t red_mask{ 0x00ff0000 };        
	uint32_t green_mask{ 0x0000ff00 };       
	uint32_t blue_mask{ 0x000000ff };        
	uint32_t alpha_mask{ 0xff000000 };       
	uint32_t color_space_type{ 0x73524742 }; 
	uint32_t unused[16]{ 0 };                
};

struct BMP {
	BMPFileHeader file_header;
	BMPInfoHeader bmp_info_header;
	BMPColorHeader bmp_color_header;
	vector<uint8_t> data;

	BMP(const char* fname) {
		read(fname);
	}

	BMP(int32_t width, int32_t height, bool has_alpha = true) {
		if (width <= 0 || height <= 0) {
			throw runtime_error("The image width and height must be positive numbers.");
		}

		bmp_info_header.width = width;
		bmp_info_header.height = height;
		if (has_alpha) {
			bmp_info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
			file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);

			bmp_info_header.bit_count = 32;
			bmp_info_header.compression = 3;
			row_stride = width * 4;
			data.resize(row_stride * height);
			file_header.file_size = file_header.offset_data + data.size();
		}
		else {
			bmp_info_header.size = sizeof(BMPInfoHeader);
			file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

			bmp_info_header.bit_count = 24;
			bmp_info_header.compression = 0;
			row_stride = width * 3;
			data.resize(row_stride * height);

			uint32_t new_stride = make_stride_aligned();
			file_header.file_size = file_header.offset_data + data.size() + bmp_info_header.height * (new_stride - row_stride);
		}
	}

	void read(const char* fname) {
		ifstream input{ fname, ios_base::binary };
		if (input)
		{
			input.read((char*)&file_header, sizeof(file_header));
			if (file_header.file_type != 0x4D42)
			{
				throw runtime_error("Error! Unrecognized file format.");
			}
		}
		input.read((char*)&bmp_info_header, sizeof(bmp_info_header));

		if (bmp_info_header.bit_count == 32) {
			if (bmp_info_header.size >= (sizeof(BMPInfoHeader) + sizeof(BMPColorHeader))) {
				input.read((char*)&bmp_color_header, sizeof(bmp_color_header));
				check_color_header(bmp_color_header);
			}
			else {
				cerr << "Warning! The file \"" << fname << "\" does not seem to contain bit mask information\n";
				throw runtime_error("Error! Unrecognized file format.");
			}
		}
		input.seekg(file_header.offset_data, input.beg);

		if (bmp_info_header.bit_count == 32) {
			bmp_info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
			file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
		}
		else {
			bmp_info_header.size = sizeof(BMPInfoHeader);
			file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
		}
		file_header.file_size = file_header.offset_data;

		data.resize(bmp_info_header.width * bmp_info_header.height * bmp_info_header.bit_count / 8);

		if (bmp_info_header.width % 4 == 0) {
			input.read((char*)data.data(), data.size());
			file_header.file_size += data.size();
		}
		else {
			row_stride = bmp_info_header.width * bmp_info_header.bit_count / 8;
			uint32_t new_stride = make_stride_aligned();
			vector<uint8_t> padding_row(new_stride - row_stride);

			for (int y = 0; y < bmp_info_header.height; ++y) {
				input.read((char*)(data.data() + row_stride * y), row_stride);
				input.read((char*)padding_row.data(), padding_row.size());
			}
			file_header.file_size += data.size() + bmp_info_header.height * padding_row.size();
		}
	}

	void write(const char* fname) {
		ofstream output{ fname, ios_base::binary };
		if (output)
		{
			if (bmp_info_header.bit_count == 32) {
				write_headers_and_data(output);
			}
			else if (bmp_info_header.bit_count == 24)
			{
				if (bmp_info_header.width % 4 == 0) {
					write_headers_and_data(output);
				}
				else {
					uint32_t new_stride = make_stride_aligned();
					vector<uint8_t> padding_row(new_stride - row_stride);

					write_headers(output);

					for (int y = 0; y < bmp_info_header.height; ++y) {
						output.write((const char*)(data.data() + row_stride * y), row_stride);
						output.write((const char*)padding_row.data(), padding_row.size());
					}
				}
			}
			else {
				throw runtime_error("The program can treat only 24 or 32 bits per pixel BMP files");
			}
		}
		else {
			throw runtime_error("Unable to open the output image file.");
		}		
	}

	void cross(uint32_t x, uint32_t y, uint32_t x2, uint32_t y2, uint8_t B, uint8_t G, uint8_t R) {
		if (x > (uint32_t)bmp_info_header.width || y > (uint32_t)bmp_info_header.height) {
			throw runtime_error("The cross does not fit in the image!");
		}
		else if ((x2 - x) != (y2 - y))
		{
			throw runtime_error("The line of cross must be same");
		}

		uint32_t channels = bmp_info_header.bit_count / 8;
		const uint32_t pos{ ((x2 - x) / 2) };

		for (uint32_t i = x; i < x2; ++i)
		{
			data[channels * (x * bmp_info_header.width + i) + 0] = B;
			data[channels * (x * bmp_info_header.width + i) + 1] = G;
			data[channels * (x * bmp_info_header.width + i) + 2] = R;
			if (channels == 4)
			{
				data[channels * (x * bmp_info_header.width + i) + 3] = 255;
			}
		}

		for (uint32_t i = y - pos; i < y2 - pos; ++i)
		{
			data[channels * (i * bmp_info_header.width + x + pos) + 0] = B;
			data[channels * (i * bmp_info_header.width + x + pos) + 1] = G;
			data[channels * (i * bmp_info_header.width + x + pos) + 2] = R;
			if (channels == 4)
			{
				data[channels * (i * bmp_info_header.width + pos) + 3] = 255;
			}
		}
	}

private:
	uint32_t row_stride{ 0 };

	uint32_t make_stride_aligned() {
		uint32_t new_stride = row_stride;
		while (new_stride % 4 != 0)
		{
			new_stride++;
		}
		return new_stride;
	}

	void check_color_header(BMPColorHeader& bmp_color_header) {
		BMPColorHeader expected_color_header;
		if (expected_color_header.red_mask != bmp_color_header.red_mask ||
			expected_color_header.blue_mask != bmp_color_header.blue_mask ||
			expected_color_header.green_mask != bmp_color_header.green_mask ||
			expected_color_header.alpha_mask != bmp_color_header.alpha_mask) {
			throw runtime_error("Unexpected color mask format! The program expects the pixel data to be in the BGRA format");
		}
		if (expected_color_header.color_space_type != bmp_color_header.color_space_type) {
			throw runtime_error("Unexpected color space type! The program expects sRGB values");
		}
	}

	void write_headers(ofstream& output) {
		output.write((const char*)&file_header, sizeof(file_header));
		output.write((const char*)&bmp_info_header, sizeof(bmp_info_header));
		if (bmp_info_header.bit_count == 32) {
			output.write((const char*)&bmp_color_header, sizeof(bmp_color_header));
		}
	}

	void write_headers_and_data(ofstream& output) {
		write_headers(output);
		output.write((const char*)data.data(), data.size());
	}
};
#pragma pack(pop)

int main()
{
	string pathToFile{};
	string pathToExport{};
	cout << "Enter the path and name in the end of path, where located your BMP file:";
	cout << endl;
	cin >> pathToFile;
	BMP bmp(pathToFile.c_str());
	cout << endl;
	cout << "Enter the path and name in the end of path, where you wanna locate your changed BMP file:";
	cout << endl;
	cin >> pathToExport;
	bmp.cross(250, 250, 300, 300, 0, 0, 255); // x y x1 y1 BGR 
	bmp.write(pathToExport.c_str());

}

