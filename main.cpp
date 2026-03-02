#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>

// Include stb libraries for image processing
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

extern "C"
{
    // Struct to pass data back to .NET safely
    struct ProcessedMediaResult
    {
        uint8_t *data;
        size_t length;
        bool isSuccess;
    };

    // Callback function to capture stb_image_write output into a std::vector
    void WriteToVectorCallback(void *context, void *data, int size)
    {
        std::vector<uint8_t> *buffer = static_cast<std::vector<uint8_t> *>(context);
        uint8_t *byteData = static_cast<uint8_t *>(data);
        buffer->insert(buffer->end(), byteData, byteData + size);
    }

    // Main PRO processing function
    __declspec(dllexport) ProcessedMediaResult ProcessImage(const uint8_t *inputData, size_t inputLength)
    {
        ProcessedMediaResult result = {nullptr, 0, false};

        // 1. DECODE & VALIDATE (Security Check)
        // stbi_load_from_memory will automatically validate the file structure.
        // If it's a corrupted file or malware disguised as an image, it will fail here.
        int width, height, channels;
        uint8_t *decodedImage = stbi_load_from_memory(inputData, static_cast<int>(inputLength), &width, &height, &channels, 4);

        if (!decodedImage)
        {
            std::cout << "[C++ ENGINE] INVALID FILE FORMAT! Security block." << std::endl;
            return result; // Return failed state
        }

        std::cout << "[C++ ENGINE] Valid image detected! Size: " << width << "x" << height << std::endl;

        // 2. RESIZE (Example: scale down if width > 1920)
        int targetWidth = width;
        int targetHeight = height;
        uint8_t *finalPixels = decodedImage; // Default to original pixels
        uint8_t *resizedImage = nullptr;

        if (width > 1920)
        {
            targetWidth = 1920;
            targetHeight = (height * 1920) / width; // Maintain aspect ratio

            // Allocate memory for resized image (4 channels = RGBA)
            resizedImage = new uint8_t[targetWidth * targetHeight * 4];

            // Perform high-quality linear resize
            stbir_resize_uint8_linear(
                decodedImage, width, height, 0,
                resizedImage, targetWidth, targetHeight, 0,
                STBIR_RGBA);

            finalPixels = resizedImage;
            std::cout << "[C++ ENGINE] Image resized to: " << targetWidth << "x" << targetHeight << std::endl;
        }

        // 3. ENCODE TO OPTIMIZED FORMAT (Using JPEG for size reduction)
        // Note: For actual WebP, You would link libwebp. For this single-header PRO approach,
        // we compress to a highly optimized JPEG at 80% quality.
        std::vector<uint8_t> outputBuffer;
        int writeSuccess = stbi_write_jpg_to_func(WriteToVectorCallback, &outputBuffer, targetWidth, targetHeight, 4, finalPixels, 80);

        if (writeSuccess)
        {
            // Allocate memory on C++ heap to pass back to .NET
            result.length = outputBuffer.size();
            result.data = new uint8_t[result.length];
            std::memcpy(result.data, outputBuffer.data(), result.length);
            result.isSuccess = true;
            std::cout << "[C++ ENGINE] Processing complete! Optimized size: " << result.length << " bytes." << std::endl;
        }

        // 4. CLEANUP INTERNAL C++ MEMORY
        stbi_image_free(decodedImage);
        if (resizedImage)
        {
            delete[] resizedImage;
        }

        return result;
    }

    // Free memory allocated for .NET interop
    __declspec(dllexport) void FreeMediaResult(ProcessedMediaResult result)
    {
        if (result.data != nullptr)
        {
            delete[] result.data;
            std::cout << "[C++ ENGINE] Native memory freed successfully." << std::endl;
        }
    }
}