#include <iostream>
#include <iomanip>
#include <sstream>
#include "MBR.hpp"
#include "FAT32.hpp"

using namespace std;



int main(int argc,char** argv) {

	if (argc < 2) {
		cerr << "Usage:\tFAT32_editor vhd_file" << endl;
		return -1;
	}

	string filename(argv[1]);

	FAT32* fs = nullptr;
	MBR* vhd = nullptr;
	int result = -1;

	do {

		try {
			vhd = new MBR(filename);
			//MBR vhd(string(argv[1]));

			for (auto i = 0; i < 4; i++) {
				if (fs = vhd->partition(i))
					break;
			}
		}
		catch (exception& e) {
			cerr << e.what() << endl;
		}
		if (!fs) {
			cerr << "No FAT32 partition found" << endl;
			break;
		}

		/*

		put inner_path outer_path


		*/

		while (true) {
			cout << "FAT32 > ";
			string str;
			getline(cin, str);
			if (!cin.good())
				break;

			stringstream ss(str);

			ss >> str;

			try {
				if (str == "list") {
					auto it = fs->list();

					while (it.step()) {

						if (it.is_folder()) {
							cout << it.fullname() << '\\' << endl;
							continue;
						}
						cout << it.fullname() << '\t';

						dword size = it.size();
						if (size < 1024)	//bytes
							cout << size << " bytes" << endl;
						else {
							static const char postfix[] = "KMG";
							unsigned level = 0;		//KB
							double adjusted_size = (double)size / 0x400;

							while (!(adjusted_size < 1024)) {
								adjusted_size /= 0x400;
								++level;
								//shouldn't bigger than 4G
								//assertless(level, 3);
							}
							cout << fixed;
							if (adjusted_size < 10)
								cout << setprecision(2);
							else if (adjusted_size < 100)
								cout << setprecision(1);
							else
								cout << setprecision(0);

							cout << adjusted_size << ' ' << postfix[level] << 'B' << endl;


						}

					}


					continue;
				}

				if (str == "put") {
					string inner_name;
					ss >> inner_name;
					ss >> str;	//outer_path

					ifstream file(str, ios::in | ios::binary);
					if (!file.is_open()) {
						cerr << "cannot open file\t" << str << endl;
						continue;
					}

					if (!fs->put(inner_name, file)) {
						cerr << "error in FAT32::put\t" << inner_name << endl;
					}
					continue;
				}
				cerr << "unknown command" << endl;
			}
			catch (exception& e) {
				cerr << e.what() << endl;
			}

		}
		result = 0;
	} while (false);

	delete vhd;

	return result;
}