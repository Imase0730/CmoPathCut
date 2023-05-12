//--------------------------------------------------------------------------------------
// File: CmoPathCut.cpp
//
// cmoファイルのcso、ddsの長いファイル名を短くするツールです。
//
// 使い方：CmoPathCut.exe </f FBXのフォルダ> </c CMOの出力フォルダ> [/b] [/a] [/s]
//
//     例：CmoPathCut.exe /f FBX /c Resources\Models　
// 
//     ※ ボーンやアニメーションクリップの入ったFBXの場合はオプション [/b] [/a] を指定してください。
//     ※ [/s]はcmoを別のプロジェクトで作成した場合パス名がわからないので、最後の"_"の前の文字列をカットします。
//        [/s]を使用する場合はDDSのファイル名に"_"を使用しないでください。
//
// Date: 2023.5.13
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------

#include <Windows.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <DirectXMath.h>
#include <VertexTypes.h>
#include <Model.h>
#include <algorithm>

using namespace DirectX;

// 関数のプロトタイプ宣言
bool getFileNames(std::wstring folderPath, std::vector<std::wstring>& file_names);
void Convert(std::wstring outfolder, std::wstring fname, bool bone, bool anime, std::wstring path);
std::wstring GetCurrentPath();
std::vector<size_t> FindAll(const std::wstring str, const std::wstring subStr);

//-----------------------------------------------------------------------------------------//
// cmoで使用されているデータの定義
//-----------------------------------------------------------------------------------------//

struct Material
{
    DirectX::XMFLOAT4   Ambient;
    DirectX::XMFLOAT4   Diffuse;
    DirectX::XMFLOAT4   Specular;
    float               SpecularPower;
    DirectX::XMFLOAT4   Emissive;
    DirectX::XMFLOAT4X4 UVTransform;
};

constexpr uint32_t MAX_TEXTURE = 8;

struct SubMesh
{
    uint32_t MaterialIndex;
    uint32_t IndexBufferIndex;
    uint32_t VertexBufferIndex;
    uint32_t StartIndex;
    uint32_t PrimCount;
};

constexpr uint32_t NUM_BONE_INFLUENCES = 4;

static_assert(sizeof(VertexPositionNormalTangentColorTexture) == 52, "mismatch with CMO vertex type");

struct SkinningVertex
{
    uint32_t boneIndex[NUM_BONE_INFLUENCES];
    float boneWeight[NUM_BONE_INFLUENCES];
};

struct MeshExtents
{
    float CenterX, CenterY, CenterZ;
    float Radius;

    float MinX, MinY, MinZ;
    float MaxX, MaxY, MaxZ;
};

struct Bone
{
    int32_t ParentIndex;
    DirectX::XMFLOAT4X4 InvBindPos;
    DirectX::XMFLOAT4X4 BindPos;
    DirectX::XMFLOAT4X4 LocalTransform;
};

//-----------------------------------------------------------------------------------------//

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        std::cout << "使い方：CmoPathCut.exe </f fbxのフォルダ> </c cmoのフォルダ> [/b] [/a] [/s]" << std::endl;
        std::cout << "/b ボーン情報有り" << std::endl;
        std::cout << "/a アニメション情報有り" << std::endl;
        std::cout << "/s 最後の_の前の文字列をカットします" << std::endl;
        return 0;
    }

    std::wstring folderPath(L".\\");
    std::wstring srcfolder;
    std::wstring outfolder;
    std::vector<std::wstring> file_names;

    bool bone = false;
    bool anime = false;
    bool s = false;
    std::wstring path;

    // オプションを取得
    std::wstring option[7];
    for (int i = 0; i < 7 && i < argc - 1; i++)
    {
        option[i] = argv[i + 1];
    }
    // オプションを反映
    for (int i = 0; i < 7 && i < argc - 1; i++)
    {
        if (option[i] == L"/f")
        {
            // 変換前のFBXのフォルダ名
            srcfolder = argv[i + 2];
            i++;
        }
        else if (option[i] == L"/c")
        {
            // 変換後のCMOのフォルダ名
            outfolder = argv[i + 2];
            folderPath += outfolder;
            i++;
        }
        else if (option[i] == L"/b")
        {
            bone = true;
        }
        else if (option[i] == L"/a")
        {
            anime = true;
        }
        else if (option[i] == L"/s")
        {
            s = true;
        }
    }

    if (s == false)
    {
        // DDSの削除するパス名を取得
        path = GetCurrentPath();
        std::vector<size_t> rc = FindAll(srcfolder, L"../");
        for (size_t i = 0; i < rc.size(); i++)
        {
            // srcfolderの "../" の数に応じてpathを修正する
            size_t pos = path.rfind(L"_");
            path.erase(pos);
            // srcfolderの "../" 部分を削除する
            srcfolder.erase(rc[i], 3);
        }
        path += L"_";
        path += srcfolder;
        path += L"_";
    }

    // フォルダ内のファイル一覧を取得する
    getFileNames(folderPath, file_names);

    // フォルダ内のcmoファイルを変換する
    for (auto& str : file_names)
    {
        if (str.find(L".cmo") != std::wstring::npos)
        {
            Convert(folderPath, str, bone, anime, path);
        }
    }

    return 0;
}

// 文字列を置き換える関数
void Replace(std::wstring& str, std::wstring target, std::wstring replacement)
{
    if (!target.empty()) {
        std::wstring::size_type pos = 0;
        while ((pos = str.find(target, pos)) != std::wstring::npos) {
            str.replace(pos, target.length(), replacement);
            pos += replacement.length();
        }
    }
}

// 指定文字列の位置を返す関数（複数可）
std::vector<size_t> FindAll(const std::wstring str, const std::wstring subStr)
{
    std::vector<size_t> result;

    size_t subStrSize = subStr.size();
    size_t pos = str.find(subStr);

    while (pos != std::wstring::npos)
    {
        result.push_back(pos);
        pos = str.find(subStr, pos + subStrSize);
    }

    return result;
}

// 現在のフォルダのパス名を取得する関数
std::wstring GetCurrentPath()
{
    wchar_t cdir[MAX_PATH + 1];
    GetCurrentDirectory(MAX_PATH + 1, cdir);
    std::wstring str = cdir;
    str.erase(str.begin(), str.begin() + 2);    // ドライブ名を削除
    Replace(str, L"_", L"__");
    Replace(str, L"\\", L"_");
    return str;
}

// uint32_t型の数を読み込みと書き込みする関数
uint32_t ReadWriteCnt(std::ifstream& ifs, std::ofstream& ofs)
{
    uint32_t cnt = 0;
    ifs.read((char*)&cnt, sizeof(uint32_t));
    ofs.write((char*)&cnt, sizeof(uint32_t));
    return cnt;
}

// 指定サイズを読み込みと書き込みする関数
void ReadWrite(std::ifstream& ifs, std::ofstream& ofs, size_t size)
{
    std::vector<wchar_t> buf(size);
    ifs.read((char*)&buf[0], size);
    ofs.write((char*)&buf[0], size);
}

// csoとddsのファイル名のパスを削除する関数
void Convert(std::wstring outfolder, std::wstring fname, bool bone, bool anime, std::wstring path)
{
    // cmoファイルのパスを含む文字列を作成
    fname = outfolder + L"\\" + fname;

    // cmoオープン
    std::ifstream ifs(fname, std::ios::in | std::ios::binary);
    if (ifs.fail())
    {
        std::cerr << "Failed to open file." << std::endl;
    }

    // 新しいcmoオープン
    std::wstring outFname = fname;
    size_t pos = outFname.rfind(L"cmo");
    outFname.replace(pos, 3, L"new");
    std::ofstream ofs(outFname, std::ios::out | std::ios::trunc | std::ios::binary);

    // メッシュ数
    uint32_t nMesh = ReadWriteCnt(ifs, ofs);
    for (size_t i = 0; i < nMesh; i++)
    {
        // メッシュ名
        uint32_t nName = ReadWriteCnt(ifs, ofs);
        ReadWrite(ifs, ofs, sizeof(wchar_t) * nName);

        // マテリアル数
        uint32_t nMats = ReadWriteCnt(ifs, ofs);
        for (size_t j = 0; j < nMats; j++)
        {
            // マテリアル名
            nName = ReadWriteCnt(ifs, ofs);
            ReadWrite(ifs, ofs, sizeof(wchar_t) * nName);

            // マテリアル
            ReadWrite(ifs, ofs, sizeof(Material));

            // ピクセルシェーダー名の文字数
            uint32_t cnt = 0;
            ifs.read((char*)&cnt, sizeof(uint32_t));
            uint32_t size = sizeof(wchar_t) * cnt;

            // ピクセルシェーダー名
            std::vector<wchar_t> buf(size);
            ifs.read((char*)&buf[0], size);
            std::wstring src(buf.data());
            std::wstring str = src;

            if (!str.empty())
            {
                // ".dgsl"を削除する
                std::wstring::size_type pos = str.rfind(L".dgsl");
                if (pos != std::wstring::npos) str.erase(pos, 5);

                // _の前の文字列は全て消す
                pos = str.rfind(L'_');
                if (pos != std::wstring::npos) str.erase(0, pos + 1);

                // ファイル名変更
                std::wstring a = outfolder + L"\\" + src;
                std::wstring b = outfolder + L"\\" + str;

                // ファイルの存在を確認する
                std::ifstream fin(a);
                if (fin)
                {
                    fin.close();
                    _wremove(b.c_str());
                    if (_wrename(a.c_str(), b.c_str()))
                    {
                        // ファイル名変更エラー
                        throw std::runtime_error("rename error");
                    }
                }
            }

            // ピクセルシェーダー名の文字数
            cnt = static_cast<uint32_t>(str.size());
            ofs.write((char*)&cnt, sizeof(uint32_t));

            // ピクセルシェーダー名
            ofs.write((char*)str.data(), sizeof(wchar_t) * cnt);

            // テクスチャ名
            for (size_t t = 0; t < MAX_TEXTURE; ++t)
            {
                // テクスチャ名の文字数
                uint32_t cnt = 0;
                ifs.read((char*)&cnt, sizeof(uint32_t));
                uint32_t size = sizeof(wchar_t) * cnt;

                // テクスチャ名
                std::vector<wchar_t> buf(size);
                ifs.read((char*)&buf[0], size);
                std::wstring src(buf.data());
                std::wstring str = src;

                if (!str.empty())
                {
                    // ".png"を削除する
                    std::wstring::size_type pos = str.rfind(L".png");
                    if (pos != std::wstring::npos) str.erase(pos, 4);

                    if (path.empty())
                    {
                        // _の前の文字列は全て消す
                        pos = str.rfind(L'_');
                        if (pos != std::wstring::npos) str.erase(0, pos + 1);
                    }
                    else
                    {
                        // パス名の文字列を全て消す
                        std::transform(path.cbegin(), path.cend(), path.begin(), tolower);
                        std::wstring tmp = str;
                        std::transform(tmp.cbegin(), tmp.cend(), tmp.begin(), tolower);
                        pos = tmp.find(path);
                        if (pos != std::wstring::npos) str.erase(pos, path.size());
                    }

                    // ファイル名変更
                    std::wstring a = outfolder + L"\\" + src;
                    std::wstring b = outfolder + L"\\" + str;

                    // ファイルの存在を確認する
                    std::ifstream fin(a);
                    if (fin)
                    {
                        fin.close();
                        _wremove(b.c_str());
                        if (_wrename(a.c_str(), b.c_str()))
                        {
                            // ファイル名変更エラー
                            throw std::runtime_error("rename error");
                        }
                    }
                }

                // テクスチャの文字数
                cnt = static_cast<uint32_t>(str.size());
                ofs.write((char*)&cnt, sizeof(uint32_t));

                // テクスチャ名
                ofs.write((char*)str.data(), sizeof(wchar_t) * cnt);
            }

            // ボーンの有無
            uint8_t bSkeleton = 0;
            ifs.read((char*)&bSkeleton, sizeof(uint8_t));
            ofs.write((char*)&bSkeleton, sizeof(uint8_t));

            // サブメッシュ
            uint32_t nSubmesh = ReadWriteCnt(ifs, ofs);
            ReadWrite(ifs, ofs, sizeof(SubMesh) * nSubmesh);

            // インデックスバッファ
            uint32_t nIBs = ReadWriteCnt(ifs, ofs);
            for (size_t j = 0; j < nIBs; ++j)
            {
                uint32_t nIndexes = ReadWriteCnt(ifs, ofs);
                ReadWrite(ifs, ofs, nIndexes * sizeof(uint16_t));
            }

            // 頂点バッファ
            uint32_t nVBs = ReadWriteCnt(ifs, ofs);
            for (size_t j = 0; j < nVBs; ++j)
            {
                uint32_t nVerts = ReadWriteCnt(ifs, ofs);
                ReadWrite(ifs, ofs, sizeof(VertexPositionNormalTangentColorTexture) * nVerts);
            }

            // スキンアニメーション用頂点バッファ
            uint32_t nSkinVBs = ReadWriteCnt(ifs, ofs);
            if (nSkinVBs)
            {
                for (size_t j = 0; j < nSkinVBs; ++j)
                {
                    uint32_t nVerts = ReadWriteCnt(ifs, ofs);
                    ReadWrite(ifs, ofs, sizeof(SkinningVertex) * nVerts);
                }
            }

            // メッシュの付加情報
            ReadWrite(ifs, ofs, sizeof(MeshExtents));

            // ボーンの入ったモデル
            if (bSkeleton && bone == true)
            {
                // ボーン数
                uint32_t nBones = ReadWriteCnt(ifs, ofs);

                for (uint32_t j = 0; j < nBones; ++j)
                {
                    // ボーン名
                    nName = ReadWriteCnt(ifs, ofs);
                    ReadWrite(ifs, ofs, sizeof(wchar_t) * nName);
                    // ボーン情報
                    ReadWrite(ifs, ofs, sizeof(Bone));
                }

                // アニメーション情報
                if (anime == true)
                {
                    // 現在の位置を取得する
                    size_t cur = ifs.tellg();
                    // ファイルポインタを末尾に移動
                    ifs.seekg(0, std::ios::end);
                    // 終端の位置を取得する
                    size_t end = ifs.tellg();
                    // 残りのサイズを取得する
                    size_t size = end - cur;
                    // 元の位置へ戻す
                    ifs.seekg(cur, std::ios::beg);
                    // 残りを読み込み＆書き出し
                    ReadWrite(ifs, ofs, size);
                }

            }
        }
    }

    ifs.close();
    ofs.close();

    // cmoを削除
    _wremove(fname.c_str());
    // newをcmoに変更
    if (_wrename(outFname.c_str(), fname.c_str()))
    {
        // ファイル名変更エラー
        throw std::runtime_error("rename error");
    }
}

// 指定フォルダのファイル一覧を取得する関数
bool getFileNames(std::wstring folderPath, std::vector<std::wstring>& file_names)
{
    HANDLE hFind;
    WIN32_FIND_DATA win32fd;
    std::wstring search_name = folderPath + L"\\*";

    hFind = FindFirstFile(search_name.c_str(), &win32fd);

    if (hFind == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("file not found");
        return false;
    }

    /* 指定のディレクトリ以下のファイル名をファイルがなくなるまで取得する */
    do {
        if (win32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* ディレクトリの場合は何もしない */
        }
        else {
            /* ファイルが見つかったらVector配列に保存する */
            file_names.push_back(win32fd.cFileName);
        }
    } while (FindNextFile(hFind, &win32fd));

    FindClose(hFind);

    return true;
}
