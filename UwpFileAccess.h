#pragma once

// UWPのファイルアクセスを行う機能を提供する
// ファイルとは
// - アプリがアクセス可能な領域に置かれたWindows::Storage::StorageFile
// - アプリに組み込まれたアセット

#include <fstream>
#include <boost/interprocess/streams/vectorstream.hpp>
#include <inspectable.h>
#include <wrl\client.h>
#include <robuffer.h>
#include "Common\DirectXHelper.h"

using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace concurrency;

class UwpFileAccess sealed
{
public:
	typedef std::function<void(std::istream& stream)>	read_callback;

private:
	static void convert_uwp2cpp_ram(IBuffer ^ buffer, read_callback cb) {
		// IBufferオブジェクトからC++標準のistreamを作りたい
		Platform::Object^ obj = buffer;
		Microsoft::WRL::ComPtr<IInspectable> insp(reinterpret_cast<IInspectable*>(obj));
		Microsoft::WRL::ComPtr<IBufferByteAccess> bufferByteAccess;
		DX::ThrowIfFailed(insp.As(&bufferByteAccess));
		byte* pmx_raw_data_ptr = nullptr;
		DX::ThrowIfFailed(bufferByteAccess->Buffer(&pmx_raw_data_ptr));

		// COMインターフェイス経由で得たアドレスを使ってvectorを生成したい
		// いい方法が思いつかないのでコピーで済ませる : todo改善ポイント
		std::vector<char>	pmx_raw_data;
		pmx_raw_data.resize(buffer->Length);
		CopyMemory(&pmx_raw_data[0], pmx_raw_data_ptr, buffer->Length);

		boost::interprocess::basic_ivectorstream<std::vector<char> >  input_vector_stream(pmx_raw_data);
		cb(input_vector_stream);
	}

public:
	UwpFileAccess();

	// アセットファイルを読み込む
	static void ReadAsset(std::wstring const & asset_file_name, read_callback cb) {
		auto asset_fullpath = L"ms-appx:///Assets/" + asset_file_name;
		auto uwp_file_name = ref new Platform::String(asset_fullpath.c_str());
		auto uri = ref new Windows::Foundation::Uri(uwp_file_name);
		create_task(StorageFile::GetFileFromApplicationUriAsync(uri)).then([cb](StorageFile ^ file) {
			if (file) {
				create_task(FileIO::ReadBufferAsync(file)).then([cb](task<IBuffer^> task) {
					auto buffer = task.get();
					auto reader = DataReader::FromBuffer(buffer);
					auto length = buffer->Length;
					auto loaded_buffer = reader->ReadBuffer(length);
					convert_uwp2cpp_ram(loaded_buffer, cb);
				});
			}
		});
	}
};

