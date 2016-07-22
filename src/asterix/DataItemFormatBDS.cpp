/*
 *  Copyright (c) 2013 Croatia Control Ltd. (www.crocontrol.hr)
 *
 *  This file is part of Asterix.
 *
 *  Asterix is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Asterix is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Asterix.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * AUTHORS: Damir Salantic, Croatia Control Ltd.
 *
 */

#if defined(PYTHON_WRAPPER)
#include <Python.h>
#endif

#include "DataItemFormatBDS.h"
#include "Tracer.h"
#include "asterixformat.hxx"

DataItemFormatBDS::DataItemFormatBDS(int id)
: DataItemFormat(id)
{
}

DataItemFormatBDS::~DataItemFormatBDS()
{
}

DataItemFormatBDS::DataItemFormatBDS(const DataItemFormatBDS& obj)
: DataItemFormat(obj.m_nID)
{
	std::list<DataItemFormat*>::iterator it = ((DataItemFormat&)obj).m_lSubItems.begin();

	while(it != obj.m_lSubItems.end())
	{
		DataItemFormat* di = (DataItemFormat*)(*it);
		m_lSubItems.push_back(di->clone());
		it++;
	}

	m_pParentFormat = obj.m_pParentFormat;
}


long DataItemFormatBDS::getLength(const unsigned char* pData)
{
	unsigned char nRepetition = *pData;
	return (1+nRepetition*8); // BDS length is always 8 bytes
}

bool DataItemFormatBDS::getText(std::string& strResult, std::string& strHeader, const unsigned int formatType, unsigned char* pData, long nLength)
{
	bool ret = false;
	int fixedLength = 7+1; // size of BDS register (including the BDS byte)
	unsigned char nRepetition = *pData;

	if (1+nRepetition*fixedLength != nLength)
	{
		Tracer::Error("Wrong length in BDS");
		return true;
	}

	pData++;

	std::string tmpStr;

	while(nRepetition--)
	{
		int BDSid = pData[7];

		// Find BDS register
		std::list<DataItemFormat*>::iterator it =  m_lSubItems.begin();
		while (it != m_lSubItems.end())
		{
			DataItemFormatFixed* pFixed = (DataItemFormatFixed*)(*it);
			if (pFixed->m_nID == BDSid || pFixed->m_nID == 0)
			{
				std::string item_str;
				if (formatType == CAsterixFormat::EJSONH)
					item_str = format("\n\t\t{\"BDS%x\":", BDSid);
				else if (formatType == CAsterixFormat::EJSON)
					item_str = format("{\"BDS%x\":", BDSid);
				else if (formatType == CAsterixFormat::ETxt)
					item_str = format("\n\tBDS register: %x", BDSid);

				bool ret2 = pFixed->getText(item_str, strHeader, formatType, pData, fixedLength-1);
				if (ret2)
				{
					ret |= ret2;

					if (formatType == CAsterixFormat::EJSONH || formatType == CAsterixFormat::EJSON)
					{
						if (tmpStr.size() == 0)
							tmpStr = "[";
						else
							tmpStr += ",";

						tmpStr += item_str + "}";
					}
					else if (formatType == CAsterixFormat::ETxt)
					{
						tmpStr += item_str;

					}
					else
					{
						tmpStr += item_str;
					}
				}
				break;
			}
			it++;
		}

		pData += fixedLength;

	}

	if (formatType == CAsterixFormat::EJSONH || formatType == CAsterixFormat::EJSON)
	{
		tmpStr += format("]");
	}

	if (ret)
		strResult += tmpStr;

	return ret;
}

std::string DataItemFormatBDS::printDescriptors(std::string header)
{
	std::string resStr;
	std::list<DataItemFormat*>::iterator it =  m_lSubItems.begin();
	while (it != m_lSubItems.end())
	{
		DataItemFormatFixed* pFixed = (DataItemFormatFixed*)(*it);
		if (pFixed == NULL)
		{
			Tracer::Error("Wrong data in BDS");
		}
		else
		{
			std::string bds_header = format("%sBDS%x:", header.c_str(), pFixed->m_nID);

			resStr += pFixed->printDescriptors(bds_header);
		}
		it++;
	}
	return resStr;
}

bool DataItemFormatBDS::filterOutItem(const char* name)
{
	int item_id = 0;
	char item_name[128] = "";

	if (sscanf(name, "BDS%x:%128s", &item_id, item_name) != 2)
		return false;

	std::list<DataItemFormat*>::iterator it =  m_lSubItems.begin();
	while (it != m_lSubItems.end())
	{
		DataItemFormatFixed* pFixed = (DataItemFormatFixed*)(*it);
		if (pFixed->m_nID == item_id)
		{
			return pFixed->filterOutItem(item_name);
		}
		it++;
	}
	return false;
}

bool DataItemFormatBDS::isFiltered(const char* name)
{// TODO
	DataItemFormatFixed* pFixed = m_lSubItems.size() ? (DataItemFormatFixed*)m_lSubItems.front() : NULL;
	if (pFixed == NULL)
	{
		Tracer::Error("Wrong data in BDS");
		return false;
	}

	return pFixed->isFiltered(name);
}

#if defined(WIRESHARK_WRAPPER) || defined(ETHEREAL_WRAPPER)
fulliautomatix_definitions* DataItemFormatBDS::getWiresharkDefinitions()
{// TODO
	DataItemFormatFixed* pFixed = m_lSubItems.size() ? (DataItemFormatFixed*)m_lSubItems.front() : NULL;
	if (!pFixed)
	{
		Tracer::Error("Wrong format of BDS item");
		return NULL;
	}
	return pFixed->getWiresharkDefinitions();
}

fulliautomatix_data* DataItemFormatBDS::getData(unsigned char* pData, long len, int byteoffset)
{// TODO
	fulliautomatix_data *lastData = NULL, *firstData = NULL;
	DataItemFormatFixed* pFixed = m_lSubItems.size() ? (DataItemFormatFixed*)m_lSubItems.front() : NULL;
	if (!pFixed)
	{
		Tracer::Error("Wrong format of BDS item");
		return NULL;
	}

	int fixedLength = pFixed->getLength(pData);
	unsigned char nRepetition = *pData;

	firstData = lastData = newDataUL(NULL, PID_REP, byteoffset, 1, nRepetition);
	byteoffset+=1;

	if (1+nRepetition*fixedLength != len)
	{
		Tracer::Error("Wrong length in BDS");
		return firstData;
	}

	pData++;

	while(nRepetition--)
	{
		lastData->next = pFixed->getData(pData, fixedLength, byteoffset);
		while(lastData->next)
			lastData = lastData->next;

		pData += fixedLength;
		byteoffset += fixedLength;
	}

	return firstData;
}
#endif

#if defined(PYTHON_WRAPPER)
PyObject* DataItemFormatBDS::getObject(unsigned char* pData, long nLength)
{
	PyObject* p = PyDict_New();
	insertToDict(p, pData, nLength);
	return p;
}

void DataItemFormatBDS::insertToDict(PyObject* p, unsigned char* pData, long nLength)
{
	int fixedLength = 7+1; // size of BDS register (including the BDS byte)
	unsigned char nRepetition = *pData;

	if (1+nRepetition*fixedLength != nLength)
	{
		Tracer::Error("Wrong length in BDS");
		return;
	}

	pData++;

	std::string tmpStr;

	while(nRepetition--)
	{
		int BDSid = pData[7];

		// Find BDS register
		std::list<DataItemFormat*>::iterator it =  m_lSubItems.begin();
		while (it != m_lSubItems.end())
		{
			DataItemFormatFixed* pFixed = (DataItemFormatFixed*)(*it);
			if (pFixed->m_nID == BDSid || pFixed->m_nID == 0)
			{
                pFixed->insertToDict(p, pData, fixedLength-1);
				break;
			}
			it++;
		}

		pData += fixedLength;
	}
}

#endif
