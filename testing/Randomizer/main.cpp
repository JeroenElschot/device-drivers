#include <iostream>
#include <sstream>
#include <cstdlib>
#include <stdio.h>
#include <fstream>
#include <string.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

/** forward declaration **/
inline bool file_exists (const std::string& name);
std::ifstream::pos_type filesize(const char* filename);
bool test_randomizer(int size, std::string file_name, bool del);

int main()
{
    std::cout << "====================================\n" << std::endl;
    std::cout << "Starting tests given the input\n" << std::endl;

    /** 500 MB **/
    if(test_randomizer(524288000,"testfile1.txt", true))
    {
        std::cout << "All tests passed for testfile1.txt" << std::endl;
        std::cout << "====================================\n" << std::endl;
    }
    else
    {
        std::cout << "Testing for file testfile1.txt failed" << std::endl;
        std::cout << "====================================\n" << std::endl;

    }

    /** 100MB **/
    if(test_randomizer(104857600,"testfile2.txt", true))
    {
        std::cout << "All tests passed for testfile2.txt" << std::endl;
        std::cout << "====================================\n" << std::endl;
    }
    else
    {
        std::cout << "Testing for file testfile2.txt failed" << std::endl;
        std::cout << "====================================\n" << std::endl;
    }

    /** 50MB **/
    if(test_randomizer(10485760,"testfile3.txt", true))
    {
        std::cout << "All tests passed for testfile4.txt" << std::endl;
        std::cout << "====================================\n" << std::endl;
    }
    else
    {
        std::cout << "Testing for file testfile3.txt failed" << std::endl;
        std::cout << "====================================\n" << std::endl;
    }

    return 0;
}

/**
* @param string size, filesize.
* @param string file_name, name of the file that should be created.
* @param bool del, if true than the file will be deleted after testing
*/
bool test_randomizer(int size, std::string file_name, bool del)
{

    bool isGood;

    if(file_exists(file_name))
    {
        remove(file_name.c_str());
    }

    std::cout << "====================================" << std::endl;
    std::cout << "Creating file " << file_name << " with a size of " << size <<std::endl;

    std::stringstream size_stream;
    size_stream << size;
    std::string temp_size_str = size_stream.str();

    std::string cmd = "head -c " + temp_size_str + " /dev/randomizer >> " + file_name;
    int code = system(cmd.c_str());

    if(code != 0)
    {
        std::cout << "An error occured: " << code << std::endl;
        isGood = false;
        return isGood;
    }
    else
    {
        std::cout << "Test_Existance: ";
        if(file_exists(file_name.c_str()))
        {
            std::cout << "SUCCESS" << std::endl;
            isGood = true;
        }
        else
        {
            std::cout << "FAILED" << std::endl;
            isGood = false;
            return isGood;
        }
        std::cout << "Test_Size: ";
        if(size == filesize(file_name.c_str()))
        {
            std::cout << "SUCCESS" << std::endl;
            isGood = true;
        }
        else
        {
            std::cout << "FAILED" << std::endl;
            isGood = false;
            return isGood;
        }

        if(del)
        {
            if( remove(file_name.c_str()) != 0 )
            {
                std::cout << "File " << file_name << " couldn't be deleted." << std::endl;
            }
        }


        std::cout << "\nPrinting time results: " << std::endl;
        std::cout << "TEST: randomizer" << std::endl;
        std::string randomTest = " time head -c " + temp_size_str + " /dev/randomizer > /dev/null";
        std::cout << randomTest << std::endl;
        system(randomTest.c_str());
        std::cout << std::endl;

        std::cout << "\nPrinting time results: " << std::endl;
        std::cout << "TEST: urandom" << std::endl;
        std::string urandomTest = " time head -c " + temp_size_str + " /dev/urandom > /dev/null";
        system(urandomTest.c_str());
        std::cout << std::endl;
    }
    return isGood;
}

/**
* @param const char* filename, name of the file
* will return the size of the given file.
*/
std::ifstream::pos_type filesize(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

/**
* @param const string&, filename.
* will return a true if the file existst
*/
inline bool file_exists (const std::string& name)
{
    return ( access( name.c_str(), F_OK ) != -1 );
}
