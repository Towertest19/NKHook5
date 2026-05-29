#include "ZipBase.h"

#include <Logging/Logger.h>

#include <Util/memstream>

#include <algorithm>

using namespace Common;
using namespace Common::Files;
using namespace Common::Logging;
using namespace Common::Logging::Logger;
namespace fs = std::filesystem;

namespace
{
	std::string NormalizeEntryPath(std::string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		return path;
	}
}

ZipBase::ZipBase() : IFile() {
	this->compressionLevel = 0;
	this->closed = true;
	this->dirty = false;
}

ZipBase::ZipBase(fs::path path, std::string password) : IFile(path, 0)
{
	this->closed = true;
	this->compressionLevel = 0;
	this->password = password;
	this->dirty = false;
	this->Open(path);
}

ZipBase::~ZipBase()
{
	this->Close();
}

bool ZipBase::Open(std::filesystem::path path)
{
	this->closed = false;
	this->dirty = false;
	IFile::Open(path);
	this->archive = ZipFile::Open(path.string());
	this->compressionLevel = 0;
	return this->archive != nullptr;
}

void ZipBase::Close() {
	if (!this->closed) {
		if (this->dirty)
			ZipFile::SaveAndClose(this->archive, this->GetPath().string());
		this->archive.reset();
	}
	this->closed = true;
	this->dirty = false;
}

size_t ZipBase::GetSize() const
{
	return this->archive->GetEntriesCount();
}

void ZipBase::SetPassword(const std::string& password)
{
	this->password = password;
}

void ZipBase::SetCompressionLevel(size_t level)
{
	this->compressionLevel = level;
}

void ZipBase::MarkDirty()
{
	this->dirty = true;
}

std::vector<std::string> ZipBase::GetEntries() const
{
	std::vector<std::string> result;
	for (size_t i = 0; i < this->archive->GetEntriesCount(); i++) {
		auto entry = this->archive->GetEntry(i);
		result.push_back(NormalizeEntryPath(entry->GetFullName()));
	}
	return result;
}

bool ZipBase::HasEntry(const std::string& entry) const {
	const std::string normalized = NormalizeEntryPath(entry);
	return std::ranges::any_of(this->GetEntries(), [&normalized](const auto& e) -> bool {
		return e == normalized;
	});
}

std::vector<uint8_t> ZipBase::ReadEntry(const std::string& entry)
{
	std::vector<uint8_t> result;
	if (this->archive == nullptr) {
		//printf("Error: pArchive was null in read");
		return result;
	}
	auto pEntry = this->archive->GetEntry(NormalizeEntryPath(entry));
	if (pEntry == nullptr)
		pEntry = this->archive->GetEntry(entry);
	if (pEntry == nullptr) {
		//printf("Error: pEntry was null in read\n");
		//printf("Entry: %s\n", entry.c_str());
		return result;
	}
	pEntry->SetPassword(this->password);
	std::istream* pStream = pEntry->GetDecompressionStream();
	if (pStream == nullptr) {
		//printf("Error: pStream was null in read\n");
		//printf("Entry: %s\n", entry.c_str());
		return result;
	}
	result = std::vector<uint8_t>(std::istreambuf_iterator<char>(*pStream), {});
	return result;
}

bool ZipBase::WriteEntry(std::string entry, std::vector<uint8_t> data)
{
	std::replace(entry.begin(), entry.end(), '/', '\\');
	if (this->archive == nullptr) {
		Print(LogLevel::ERR, "pArchive was null in write");
		return false;
	}
	ZipArchiveEntry::Ptr pEntry;
	if (this->HasEntry(entry)) {
		pEntry = this->archive->GetEntry(entry);
	}
	else {
		pEntry = this->archive->CreateEntry(entry);
	}
	memstream writeStream(data.data(), data.size() * sizeof(uint8_t));
	DeflateMethod::Ptr method = DeflateMethod::Create();
	DeflateMethod::CompressionLevel level;
	switch (this->compressionLevel) {
	default:
	case 0:
	case 1:
		level = DeflateMethod::CompressionLevel::L1;
		break;
	case 2:
		level = DeflateMethod::CompressionLevel::L2;
		break;
	case 3:
		level = DeflateMethod::CompressionLevel::L3;
		break;
	case 4:
		level = DeflateMethod::CompressionLevel::L4;
		break;
	case 5:
		level = DeflateMethod::CompressionLevel::L5;
		break;
	case 6:
		level = DeflateMethod::CompressionLevel::L6;
		break;
	case 7:
		level = DeflateMethod::CompressionLevel::L7;
		break;
	case 8:
		level = DeflateMethod::CompressionLevel::L8;
		break;
	case 9:
		level = DeflateMethod::CompressionLevel::L9;
		break;
	}
	method->SetCompressionLevel(level);
	
	if (!this->password.empty()) {
		pEntry->SetPassword(this->password);
		pEntry->UseDataDescriptor();
	}
	pEntry->SetCompressionStream(writeStream, method, ZipArchiveEntry::CompressionMode::Immediate);
	this->dirty = true;

	return true;
}
