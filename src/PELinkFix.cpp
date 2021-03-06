// PELinkFix.cpp
// Copyright (c) 2015-2018 Arkadiusz Bokowy
//
// This file is a part of WinPEFix.
//
// This project is licensed under the terms of the MIT license.

#include "PELinkFix.h"

#include "mswinpe.h"


PELinkFix::PELinkFix(std::string filename) :
		m_filename(filename),
		m_file(filename, std::fstream::binary | std::fstream::in | std::fstream::out),
		m_error(ErrorCode::NoErrorCode) {

}

PELinkFix::~PELinkFix() {
	m_file.close();
}

bool PELinkFix::process() {
	if (!checkFileFormat())
		return false;
	if (!createBackupFile())
		return false;
	return updateSystemVersion();
}

std::string PELinkFix::getBackupFileName() const {
	return m_filename + ".old";
}

bool PELinkFix::checkFileFormat() {

	if (!m_file.is_open()) {
		m_error = ErrorCode::FileOpenError;
		return false;
	}

	IMAGE_DOS_HEADER dosHeader;
	IMAGE_NT_HEADERS ntHeaders;

	m_file.seekg(0);
	m_file.read((char *)&dosHeader, sizeof(dosHeader));
	if (!m_file || m_file.gcount() != sizeof(dosHeader))
		goto return_invalid;

	if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
		goto return_invalid;

	m_file.seekg(dosHeader.e_lfanew);
	m_file.read((char *)&ntHeaders, sizeof(ntHeaders));
	if (!m_file || m_file.gcount() != sizeof(ntHeaders))
		goto return_invalid;

	if (ntHeaders.Signature != IMAGE_NT_SIGNATURE)
		goto return_invalid;

	return true;

return_invalid:
	m_error = ErrorCode::InvalidPEFormat;
	return false;
}

bool PELinkFix::updateSystemVersion() {

	if (!m_file.is_open()) {
		m_error = ErrorCode::FileOpenError;
		return false;
	}

	IMAGE_DOS_HEADER dosHeader;
	IMAGE_NT_HEADERS ntHeaders;
	long version;

	m_file.seekg(0);
	m_file.read((char *)&dosHeader, sizeof(dosHeader));
	m_file.seekg(dosHeader.e_lfanew);
	m_file.read((char *)&ntHeaders, sizeof(ntHeaders));

	version = ntHeaders.OptionalHeader.MajorOperatingSystemVersion * 0xFFFF +
		ntHeaders.OptionalHeader.MinorLinkerVersion;
	if (version > 0x50001) { // downgrade operating system version
		ntHeaders.OptionalHeader.MajorOperatingSystemVersion = 5;
		ntHeaders.OptionalHeader.MinorOperatingSystemVersion = 1;
	}

	version = ntHeaders.OptionalHeader.MajorSubsystemVersion * 0xFFFF +
		ntHeaders.OptionalHeader.MinorSubsystemVersion;
	if (version > 0x50001) { // downgrade subsystem version
		ntHeaders.OptionalHeader.MajorSubsystemVersion = 5;
		ntHeaders.OptionalHeader.MinorSubsystemVersion = 1;
	}

	m_file.seekp(dosHeader.e_lfanew);
	m_file.write((char *)&ntHeaders, sizeof(ntHeaders));

	return true;
}

bool PELinkFix::createBackupFile() {

	std::string fileName = getBackupFileName();
	std::ofstream newFile(fileName, std::fstream::binary | std::fstream::trunc);

	if (!newFile.is_open()) {
		m_error = ErrorCode::CreateBackupError;
		return false;
	}

	m_file.seekg(0);
	newFile << m_file.rdbuf();

	return true;
}

std::string PELinkFix::getErrorString() {
	switch (m_error) {
	case ErrorCode::FileOpenError:
		return "Unable to open file for read and write";
	case ErrorCode::CreateBackupError:
		return "Unable to create backup file";
	case ErrorCode::InvalidPEFormat:
		return "Not a PE format";
	default:
		return "No error";
	}
}
