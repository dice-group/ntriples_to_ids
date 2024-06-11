#include <fstream>
#include <iostream>
#include <btree/map.h>
#include <format>
#include <filesystem>


namespace fs = std::filesystem;

using str2id_type = btree::map<std::string, uint64_t>;

std::tuple<uint64_t, uint64_t, uint64_t> parse_line(std::string& line,
                                                    str2id_type& entity2id,
                                                    str2id_type& relation2id)
{
    static uint64_t next_entity_id = 0;
    static uint64_t next_relation_id = 0;
    static std::string s(512, '\0');
    static std::string p(512, '\0');
    static std::string o(512, '\0');

    {
        auto extract_triple_element = [](std::string& out,
                                         std::string_view remaining_line) -> std::string_view
        {
            auto const sep_pos = remaining_line.find_first_of(' ');
            if (sep_pos == std::string::npos)
            {
                throw std::runtime_error(std::format("Missing space separator."));
            }
            out.assign(remaining_line.substr(0, sep_pos));
            if (!(out.starts_with('<') && out.ends_with('>')))
            {
                throw std::runtime_error(std::format("Missing <> around entity or relation: {}", out));
            }
            return remaining_line.substr(sep_pos + 1);
        };

        auto remaining_line = std::string_view{line};
        remaining_line = extract_triple_element(s, remaining_line);
        remaining_line = extract_triple_element(p, remaining_line);
        remaining_line = extract_triple_element(o, remaining_line);
        if (remaining_line != ".")
        {
            throw std::runtime_error(std::format("Suspicious line end: {}", remaining_line));
        }
    }

    auto const map_string = [](auto& str2id, auto const& str, auto& next_id) -> size_t
    {
        auto [it, was_added] = str2id.try_emplace(str, next_id);
        if (was_added)
            ++next_id;
        return it->second;
    };
    auto const s_id = map_string(entity2id, s, next_entity_id);
    auto const p_id = map_string(relation2id, p, next_relation_id);
    auto const o_id = map_string(entity2id, o, next_entity_id);

    return {s_id, p_id, o_id};
}


void write_map_to_csv(fs::path const& csv_file_path, str2id_type const& map)
{
    uint64_t mappings_written = 0;
    std::ofstream outfile(csv_file_path);
    for (auto const& [relation, id] : map)
    {
        outfile << std::format("{},{}\n", relation, id);
        if (++mappings_written % 10'000'000UL == 0)
        {
            outfile.flush();
            std::cerr << std::format("{} mappings exported.", mappings_written) << std::endl;
        }
    }
    outfile.flush();
};


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input file> <output folder>" << std::endl;
        return 1;
    }

    std::string input_file_path = argv[1];
    std::string output_folder_path = argv[2];

    // Check if file exists
    if (!fs::exists(input_file_path) || !fs::is_regular_file(input_file_path))
    {
        std::cerr << "Error: The file " << input_file_path << " does not exist." << std::endl;
        return 1;
    }
    auto file_stem = fs::path{input_file_path}.stem().string();

    // Check if folder exists
    if (!fs::exists(output_folder_path) || !fs::is_directory(output_folder_path))
    {
        std::cerr << "Error: The folder " << output_folder_path << " does not exist." << std::endl;
        return 1;
    }
    fs::path output_folder{output_folder_path};

    str2id_type entity2id;
    str2id_type relation2id;
    uint64_t line_number = 0;
    std::string line;
    try
    {
        std::ifstream infile(input_file_path);
        std::ofstream outfile(output_folder / format("{}.ids.csv", file_stem));
        while (std::getline(infile, line))
        {
            ++line_number;
            try
            {
                auto [s,p,o] = parse_line(line, entity2id, relation2id);
                outfile << std::format("{},{},{}\n", s, p, o);
                if (line_number % 10'000'000UL == 0)
                {
                    outfile.flush();
                    std::cerr <<
                        std::format("{} lines translated.\n"
                                    "{} entities found.\n"
                                    "{} relations found.\n",
                                    line_number, entity2id.size(), relation2id.size())
                        << std::endl;
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << std::format("Error parsing line {}\n{}", line_number, e.what()) << std::endl;
                std::cerr << std::format("line str:\n{}\n", line) << std::endl;
            }
        }
        outfile.flush();
    }
    catch (std::exception const& e)
    {
        std::cerr << std::format("Error indexing ntriple file on line {}\n{}", line_number, e.what()) << std::endl;
        std::cerr << std::format("line str:\n{}\n", line) << std::endl;
    }

    std::cerr << "Finished indexing the ntriple file. " << std::endl;
    std::cerr <<
        std::format("Total data processed:\n"
                    "{} lines translated.\n"
                    "{} entities found.\n"
                    "{} relations found.\n",
                    line_number, entity2id.size(), relation2id.size())
        << std::endl;

    auto const dump_map = [&](auto const& name, auto const& map)
    {
        std::cerr << std::format("Start dumping {} mapping. ", name) << std::endl;
        write_map_to_csv(output_folder / format("{}.{}.csv", file_stem, name), map);
        std::cerr << std::format("Finished dumping {} mapping. ", name) << std::endl;
    };
    dump_map("entity2id", entity2id);
    dump_map("relation2id", relation2id);
}
