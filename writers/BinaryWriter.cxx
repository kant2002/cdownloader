/*
 * cdownload lib: downloads, unpacks, and reads data from the Cluster CSA arhive
 * Copyright (C) 2016  Eugene Shalygin <eugene.shalygin@gmail.com>
 *
 * The development was partially supported by the Volkswagen Foundation
 * (VolkswagenStiftung).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "./BinaryWriter.hxx"

#include "../util.hxx"
#include "../field.hxx"

#include <cstdio>
#include <fstream>
#include <type_traits>

#include "config.h"

namespace datatypenaming {
extern const std::string IEEE_REAL;
extern const std::string PC_REAL;
extern const std::string VAX_REAL;
extern const std::string VAXG_REAL;
extern const std::string LSB_INTEGER;
extern const std::string PC_INTEGER;
extern const std::string LSB_UNSIGNED_INTEGER;
extern const std::string UNSIGNED_INTEGER;
extern const std::string MSB_INTEGER;
extern const std::string MSB_UNSIGNED_INTEGER;

extern const std::string NATIVE_INT;
extern const std::string NATIVE_UNSIGNED_INT;
extern const std::string NATIVE_REAL;
}

const std::string datatypenaming::IEEE_REAL = std::string("IEEE_REAL");
const std::string datatypenaming::PC_REAL = std::string("PC_REAL");
const std::string datatypenaming::VAX_REAL = std::string("VAX_REAL");
const std::string datatypenaming::VAXG_REAL = std::string("VAXG_REAL");
const std::string datatypenaming::LSB_INTEGER = std::string("LSB_INTEGER");
const std::string datatypenaming::PC_INTEGER = std::string("PC_INTEGER");
const std::string datatypenaming::LSB_UNSIGNED_INTEGER = std::string("LSB_UNSIGNED_INTEGER");
const std::string datatypenaming::UNSIGNED_INTEGER = std::string("UNSIGNED_INTEGER");
const std::string datatypenaming::MSB_INTEGER = std::string("MSB_INTEGER");
const std::string datatypenaming::MSB_UNSIGNED_INTEGER = std::string("MSB_UNSIGNED_INTEGER");


#if SYSTEM_IS_BIG_ENDIAN
const std::string datatypenaming::NATIVE_INT = datatypenaming::MSB_INTEGER;
const std::string datatypenaming::NATIVE_UNSIGNED_INT = datatypenaming::MSB_UNSIGNED_INTEGER;
const std::string datatypenaming::NATIVE_REAL = datatypenaming::IEEE_REAL;
#else
const std::string datatypenaming::NATIVE_INT = datatypenaming::LSB_INTEGER;
const std::string datatypenaming::NATIVE_UNSIGNED_INT = datatypenaming::LSB_UNSIGNED_INTEGER;
const std::string datatypenaming::NATIVE_REAL = datatypenaming::PC_REAL;
#endif

void cdownload::BinaryWriter::FileClose::operator()(::FILE* f) const
{
	std::fclose(f);
}

cdownload::BinaryWriter::BinaryWriter() = default;

cdownload::BinaryWriter::~BinaryWriter() = default;

void cdownload::BinaryWriter::open(const path& fileName)
{
	output_.reset(std::fopen(fileName.c_str(), "a+"));
	std::fseek(output_.get(), 0, SEEK_END);
	outputFileName_ = fileName;
}

void cdownload::BinaryWriter::truncate()
{
	output_.reset(std::fopen(outputFileName_.c_str(), "w+"));
}

void cdownload::BinaryWriter::write(std::size_t cellNumber, const datetime& dt,
                                   const std::vector<AveragedVariable>& cells)
{
	using namespace cdownload::csa_time_formatting;

	std::fwrite(&cellNumber, sizeof(std::size_t), 1, output_.get());

	static const datetime EPOCH = makeDateTime(1970, 1, 1, 0, 0, 0.);
	const auto dtMillisec = (dt-EPOCH).total_milliseconds();

	std::fwrite(&dtMillisec, sizeof(dtMillisec), 1, output_.get());

	struct CellValues {
		double mean;
		std::size_t count;
		double stdDev;
	};
	for (const Field& f: fields()) {
		const AveragedVariable& av = f.data(cells);
		for (const AveragingRegister& ac: av) {
			CellValues cv {ac.mean(), ac.count(),  ac.stdDev()};
			std::fwrite(&cv, sizeof(cv), 1, output_.get());
		}
	}
}


namespace {

	void printFieldHeader(std::ostream& os, const std::string& name, std::size_t elementsCount)
	{
		using cdownload::AveragingRegister;
		if (elementsCount == 1) {
			os << '\t' << name << ":mean <" << datatypenaming::NATIVE_REAL << "["
				<< sizeof(AveragingRegister::mean_value_type) * CHAR_BIT << "]>"
				<< '\t' << name << ":count <" << datatypenaming::NATIVE_UNSIGNED_INT<< "["
				<< sizeof(AveragingRegister::counter_type) * CHAR_BIT << "]>"
				<< '\t' << name << ":stddev <" << datatypenaming::NATIVE_REAL << "["
				<< sizeof(AveragingRegister::stddev_value_type) * CHAR_BIT << "]>";
		} else {
			for (std::size_t elem = 0; elem < elementsCount; ++elem) {
				os << '\t' << name << "___" << elem +1 << ":mean <" << datatypenaming::NATIVE_REAL << "["
					<< sizeof(AveragingRegister::mean_value_type) * CHAR_BIT << "]>"
					<< '\t' << name << "___" << elem +1 << ":count <" << datatypenaming::NATIVE_UNSIGNED_INT << "["
					<< sizeof(AveragingRegister::counter_type) * CHAR_BIT << "]>"
					<< '\t' << name << "___" << elem +1 << ":stddev <" << datatypenaming::NATIVE_REAL << "["
					<< sizeof(AveragingRegister::stddev_value_type) * CHAR_BIT << "]>";
			}
		}
	}
}

void cdownload::BinaryWriter::writeHeader()
{
	string headerFileName = outputFileName_.string() + ".hdr";

	std::ofstream headerFile(headerFileName.c_str());
	// writing a header line
	headerFile << "CellNo <[" <<
		datatypenaming::NATIVE_UNSIGNED_INT << sizeof(std::size_t) * CHAR_BIT << "]>\t" <<
		"MidTime <[" <<
		datatypenaming::NATIVE_UNSIGNED_INT << sizeof(decltype(timeduration().total_milliseconds())) * CHAR_BIT << "]>";
	for (const FieldDesc& f: fields()) {
		printFieldHeader(headerFile, f.name().name(), f.elementCount());
	}
	headerFile << std::endl;
}


void cdownload::BinaryWriter::initialize(const std::vector<Field>& fields)
{
	stride_ = sizeof(std::size_t) + sizeof(decltype(timeduration().total_milliseconds()));
		const std::size_t elementSize = (sizeof(AveragingRegister::mean_value_type) +
				sizeof(AveragingRegister::counter_type) +
				sizeof(AveragingRegister::stddev_value_type));
	for (const FieldDesc& f: fields) {
		std::size_t fieldSize = elementSize * f.elementCount();
		stride_ += fieldSize;
	}
	base::initialize(fields);
}

bool cdownload::BinaryWriter::canAppend(std::size_t& lastWrittenCellNumber)
{
	auto curPos = std::ftell(output_.get());
	auto signedStride = static_cast<decltype(curPos)>(stride_);
	if (curPos > signedStride) {
		std::fseek(output_.get(), -signedStride, SEEK_CUR);
		auto read = std::fread(&lastWrittenCellNumber, sizeof(std::size_t), 1, output_.get());
		std::fseek(output_.get(), curPos, SEEK_SET);
		return read == 1;
	}
	return false;
}
