#pragma once
// Простой JSON-парсер (header-only).
// Используется внутри RuntimeOffsets::LoadFromJson.

#include <cstdint>
#include <map>
#include <string>
#include <fstream>
#include <sstream>

class SimpleJsonParser
{
public:
	static bool LoadOffsets(const std::string& jsonPath, std::map<std::string, std::uint32_t>& offsets)
	{
		std::ifstream file(jsonPath);
		if (!file.is_open())
			return false;

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string content = buffer.str();
		file.close();

		std::size_t pos = 0;
		while ((pos = content.find("\"dw", pos)) != std::string::npos)
		{
			std::size_t nameStart = pos + 1;
			std::size_t nameEnd = content.find("\"", nameStart);
			if (nameEnd == std::string::npos) break;

			std::string name = content.substr(nameStart, nameEnd - nameStart);

			std::size_t valueStart = content.find(":", nameEnd);
			if (valueStart == std::string::npos) break;
			valueStart++;

			while (valueStart < content.length() && (content[valueStart] == ' ' || content[valueStart] == '\t'))
				valueStart++;

			std::size_t valueEnd = valueStart;
			while (valueEnd < content.length() && (content[valueEnd] >= '0' && content[valueEnd] <= '9'))
				valueEnd++;

			if (valueEnd > valueStart)
			{
				std::string valueStr = content.substr(valueStart, valueEnd - valueStart);
				std::uint32_t value = static_cast<std::uint32_t>(std::stoul(valueStr));
				offsets[name] = value;
			}

			pos = valueEnd;
		}

		return !offsets.empty();
	}

	static bool LoadButtons(const std::string& jsonPath, std::map<std::string, std::uint32_t>& buttons)
	{
		std::ifstream file(jsonPath);
		if (!file.is_open()) return false;
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string content = buffer.str();
		file.close();

		auto ParseKey = [&](const std::string& key) {
			std::size_t pos = content.find("\"" + key + "\"");
			if (pos != std::string::npos) {
				std::size_t colon = content.find(":", pos);
				if (colon != std::string::npos) {
					std::size_t valStart = colon + 1;
					while (valStart < content.length() && (content[valStart] == ' ' || content[valStart] == '\t')) valStart++;
					std::size_t valEnd = valStart;
					while (valEnd < content.length() && content[valEnd] >= '0' && content[valEnd] <= '9') valEnd++;
					if (valEnd > valStart) {
						buttons[key] = static_cast<std::uint32_t>(std::stoul(content.substr(valStart, valEnd - valStart)));
					}
				}
			}
		};

		ParseKey("attack");
		ParseKey("jump");
		ParseKey("left");
		ParseKey("right");
		return !buttons.empty();
	}

	static bool LoadSchemaOffsets(const std::string& jsonPath, const std::string& className, std::map<std::string, std::uint32_t>& offsets)
	{
		std::ifstream file(jsonPath);
		if (!file.is_open())
			return false;

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string content = buffer.str();
		file.close();

		std::string classSearch = "\"" + className + "\"";
		std::size_t classPos = content.find(classSearch);
		if (classPos == std::string::npos)
			return false;

		std::size_t fieldsPos = content.find("\"fields\"", classPos);
		if (fieldsPos == std::string::npos)
			return false;

		std::size_t fieldsStart = content.find("{", fieldsPos);
		if (fieldsStart == std::string::npos)
			return false;

		int braceCount = 1;
		std::size_t fieldsEnd = fieldsStart + 1;
		while (fieldsEnd < content.length() && braceCount > 0)
		{
			if (content[fieldsEnd] == '{') braceCount++;
			else if (content[fieldsEnd] == '}') braceCount--;
			fieldsEnd++;
		}

		std::string fieldsSection = content.substr(fieldsStart, fieldsEnd - fieldsStart);

		std::size_t pos = 0;
		while ((pos = fieldsSection.find("\"m_", pos)) != std::string::npos)
		{
			std::size_t nameStart = pos + 1;
			std::size_t nameEnd = fieldsSection.find("\"", nameStart);
			if (nameEnd == std::string::npos) break;

			std::string name = fieldsSection.substr(nameStart, nameEnd - nameStart);

			std::size_t valueStart = fieldsSection.find(":", nameEnd);
			if (valueStart == std::string::npos) break;
			valueStart++;

			while (valueStart < fieldsSection.length() && (fieldsSection[valueStart] == ' ' || fieldsSection[valueStart] == '\t'))
				valueStart++;

			std::size_t valueEnd = valueStart;
			while (valueEnd < fieldsSection.length() && (fieldsSection[valueEnd] >= '0' && fieldsSection[valueEnd] <= '9'))
				valueEnd++;

			if (valueEnd > valueStart)
			{
				std::string valueStr = fieldsSection.substr(valueStart, valueEnd - valueStart);
				std::uint32_t value = static_cast<std::uint32_t>(std::stoul(valueStr));
				offsets[name] = value;
			}

			pos = valueEnd;
		}

		return !offsets.empty();
	}
};
