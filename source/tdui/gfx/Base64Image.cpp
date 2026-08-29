/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Utils/Base64Image.cpp
 *  PURPOSE:     Decode embedded base64 images into D3D textures
 *
 *****************************************************************************/

#include "Base64Image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <d3d9.h>
#include <cstring>

namespace Base64Image {

    static const std::string base64Chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    static inline bool IsBase64(unsigned char c)
    {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }

    std::vector<uint8_t> Decode(const std::string& base64String)
    {
        std::vector<uint8_t> result;

        int inLen = static_cast<int>(base64String.size());
        int i = 0;
        int j = 0;
        int in_ = 0;
        unsigned char charArray4[4], charArray3[3];

        while (inLen-- && (base64String[in_] != '=') && IsBase64(base64String[in_]))
        {
            charArray4[i++] = base64String[in_];
            in_++;

            if (i == 4)
            {
                for (i = 0; i < 4; i++)
                {
                    charArray4[i] = static_cast<unsigned char>(base64Chars.find(charArray4[i]));
                }

                charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
                charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
                charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

                for (i = 0; i < 3; i++)
                {
                    result.push_back(charArray3[i]);
                }
                i = 0;
            }
        }

        if (i)
        {
            for (j = i; j < 4; j++)
            {
                charArray4[j] = 0;
            }

            for (j = 0; j < 4; j++)
            {
                charArray4[j] = static_cast<unsigned char>(base64Chars.find(charArray4[j]));
            }

            charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
            charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
            charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

            for (j = 0; j < i - 1; j++)
            {
                result.push_back(charArray3[j]);
            }
        }

        return result;
    }

    std::string Encode(const uint8_t* data, size_t length)
    {
        std::string result;
        int i = 0;
        int j = 0;
        unsigned char charArray3[3];
        unsigned char charArray4[4];

        while (length--)
        {
            charArray3[i++] = *(data++);
            if (i == 3)
            {
                charArray4[0] = (charArray3[0] & 0xfc) >> 2;
                charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
                charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
                charArray4[3] = charArray3[2] & 0x3f;

                for (i = 0; i < 4; i++)
                {
                    result += base64Chars[charArray4[i]];
                }
                i = 0;
            }
        }

        if (i)
        {
            for (j = i; j < 3; j++)
            {
                charArray3[j] = '\0';
            }

            charArray4[0] = (charArray3[0] & 0xfc) >> 2;
            charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
            charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
            charArray4[3] = charArray3[2] & 0x3f;

            for (j = 0; j < i + 1; j++)
            {
                result += base64Chars[charArray4[j]];
            }

            while (i++ < 3)
            {
                result += '=';
            }
        }

        return result;
    }

    std::string Encode(const std::vector<uint8_t>& data)
    {
        return Encode(data.data(), data.size());
    }

    std::vector<uint8_t> LoadImageFromBase64(const std::string& base64String, int& outWidth, int& outHeight, int& outChannels)
    {
        std::vector<uint8_t> result;

        std::string base64Data = base64String;
        size_t commaPos = base64Data.find(',');
        if (commaPos != std::string::npos)
        {
            base64Data = base64Data.substr(commaPos + 1);
        }

        std::vector<uint8_t> imageFileData = Decode(base64Data);
        if (imageFileData.empty())
        {
            return result;
        }

        int width, height, channels;
        unsigned char* pixels = stbi_load_from_memory(
            imageFileData.data(),
            static_cast<int>(imageFileData.size()),
            &width,
            &height,
            &channels,
            4
        );

        if (!pixels)
        {
            return result;
        }

        outWidth = width;
        outHeight = height;
        outChannels = 4;

        size_t dataSize = static_cast<size_t>(width) * height * 4;
        result.resize(dataSize);
        memcpy(result.data(), pixels, dataSize);

        stbi_image_free(pixels);

        return result;
    }

    void* CreateTextureFromBase64(void* pDevice, const std::string& base64String, int& outWidth, int& outHeight)
    {
        if (!pDevice)
        {
            return nullptr;
        }

        int channels;
        std::vector<uint8_t> pixelData = LoadImageFromBase64(base64String, outWidth, outHeight, channels);
        if (pixelData.empty())
        {
            return nullptr;
        }

        IDirect3DDevice9* d3dDevice = static_cast<IDirect3DDevice9*>(pDevice);
        IDirect3DTexture9* texture = nullptr;

        HRESULT hr = d3dDevice->CreateTexture(
            outWidth,
            outHeight,
            1,
            0,
            D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED,
            &texture,
            nullptr
        );

        if (FAILED(hr) || !texture)
        {
            return nullptr;
        }

        D3DLOCKED_RECT lockedRect;
        hr = texture->LockRect(0, &lockedRect, nullptr, 0);
        if (FAILED(hr))
        {
            texture->Release();
            return nullptr;
        }

        uint8_t* dst = static_cast<uint8_t*>(lockedRect.pBits);
        const uint8_t* src = pixelData.data();

        for (int y = 0; y < outHeight; y++)
        {
            uint8_t* dstRow = dst + y * lockedRect.Pitch;
            const uint8_t* srcRow = src + y * outWidth * 4;

            for (int x = 0; x < outWidth; x++)
            {
                dstRow[x * 4 + 0] = srcRow[x * 4 + 2];
                dstRow[x * 4 + 1] = srcRow[x * 4 + 1];
                dstRow[x * 4 + 2] = srcRow[x * 4 + 0];
                dstRow[x * 4 + 3] = srcRow[x * 4 + 3];
            }
        }

        texture->UnlockRect(0);

        return texture;
    }

    namespace {
    std::string& GetEmbeddedImageBase64Storage()
    {
        static std::string s;
        return s;
    }
    const char EMBEDDED_IMAGE_PART1[] = "";
    const char EMBEDDED_IMAGE_PART2[] = "";
    const char EMBEDDED_IMAGE_PART3[] = "";
    const char EMBEDDED_IMAGE_PART4[] = "";
    }

    const std::string& GetEmbeddedImageBase64()
    {
        std::string& s = GetEmbeddedImageBase64Storage();
        if (s.empty())
        {
            s += EMBEDDED_IMAGE_PART1;
            s += EMBEDDED_IMAGE_PART2;
            s += EMBEDDED_IMAGE_PART3;
            s += EMBEDDED_IMAGE_PART4;
        }
        return s;
    }

    void ClearEmbeddedImageData()
    {
        std::string& s = GetEmbeddedImageBase64Storage();
        s.clear();
        s.shrink_to_fit();
    }

} // namespace Base64Image
