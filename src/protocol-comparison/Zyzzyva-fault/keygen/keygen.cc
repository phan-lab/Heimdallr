#include <sodium.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

void print_usage(const char *program)
{
    std::cerr << "Usage: " << program << " <n>\n"
              << "Generates n key pairs for upstream and n key pairs for downstream\n";
}

bool save_to_file(const std::string &filename, const unsigned char *data, size_t len)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file)
    {
        return false;
    }
    file.write(reinterpret_cast<const char *>(data), len);
    return true;
}

bool generate_and_save_keypair(const std::string &dir, size_t index)
{
    unsigned char public_key[crypto_sign_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_sign_SECRETKEYBYTES]; // Note: 64 bytes for signing

    crypto_sign_keypair(public_key, secret_key);

    std::string pub_filename = dir + "/public_" + std::to_string(index) + ".key";
    std::string sec_filename = dir + "/secret_" + std::to_string(index) + ".key";

    bool success = save_to_file(pub_filename, public_key, crypto_sign_PUBLICKEYBYTES) &&
                   save_to_file(sec_filename, secret_key, crypto_sign_SECRETKEYBYTES);

    // Clear sensitive data from memory
    sodium_memzero(secret_key, crypto_sign_SECRETKEYBYTES);

    return success;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    // Parse n
    int n;
    try
    {
        n = std::stoi(argv[1]);
        if (n < 0)
        {
            throw std::out_of_range("n must be non-negative");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Invalid input: " << e.what() << std::endl;
        return 1;
    }

    // Initialize libsodium
    if (sodium_init() < 0)
    {
        std::cerr << "Failed to initialize libsodium" << std::endl;
        return 1;
    }

    // Create directory structure
    std::filesystem::create_directory("keys");
    std::filesystem::create_directory("keys/upstream");
    std::filesystem::create_directory("keys/downstream");

    // Generate upstream key pairs
    std::cout << "Generating upstream keys..." << std::endl;
    for (int i = 0; i < n; i++)
    {
        if (!generate_and_save_keypair("keys/upstream", i))
        {
            std::cerr << "Failed to save upstream key pair " << i << std::endl;
            return 1;
        }
        std::cout << "Generated upstream pair " << i + 1 << " of " << n << "\r" << std::flush;
    }

    // Generate downstream key pairs
    std::cout << "\nGenerating downstream keys..." << std::endl;
    for (int i = 0; i < n; i++)
    {
        if (!generate_and_save_keypair("keys/downstream", i))
        {
            std::cerr << "Failed to save downstream key pair " << i << std::endl;
            return 1;
        }
        std::cout << "Generated downstream pair " << i + 1 << " of " << n << "\r" << std::flush;
    }

    std::cout << "\nAll key pairs generated successfully" << std::endl;
    std::cout << "upstream keys in: keys/upstream/" << std::endl;
    std::cout << "downstream keys in: keys/downstream/" << std::endl;
    return 0;
}