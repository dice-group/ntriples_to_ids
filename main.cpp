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
        auto after_s = line.find_first_of(' ');
        s.clear();
        s.append(line.c_str(), after_s);

        auto after_p = line.find_first_of(' ', after_s + 1);
        p.clear();
        p.append(line.c_str() + after_s + 1, after_p - after_s - 1);

        auto after_o = line.find_first_of(' ', after_p + 1);
        o.clear();
        o.append(line.c_str() + after_p + 1, after_o - after_p - 1);
    }

    auto const s_id = [&]()
    {
        auto [it_s, added_s] = entity2id.try_emplace(s, next_entity_id);
        if (added_s)
            next_entity_id++;
        return it_s->second;
    }();

    auto const p_id = [&]()
    {
        auto [it_p, added_p] = relation2id.try_emplace(p, next_relation_id);
        if (added_p)
            next_relation_id++;
        return it_p->second;
    }();
    auto const o_id = [&]()
    {
        auto [it_o, added_o] = entity2id.try_emplace(o, next_entity_id);
        if (added_o)
            next_entity_id++;
        return it_o->second;
    }();
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
    {
        std::ifstream infile(input_file_path);
        // std::ofstream outfile_bin(output_folder / "some.ids.bin", std::ios::binary);
        std::ofstream outfile(output_folder / format("{}.ids.csv", file_stem));
        std::string line;
        while (std::getline(infile, line))
        {
            auto [s,p,o] = parse_line(line, entity2id, relation2id);
            outfile << std::format("{},{},{}\n", s, p, o);
            // outfile_bin.write(reinterpret_cast<const char*>(&s), sizeof(s));
            // outfile_bin.write(reinterpret_cast<const char*>(&p), sizeof(p));
            // outfile_bin.write(reinterpret_cast<const char*>(&o), sizeof(o));
            if (++line_number % 10'000'000UL == 0)
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
        outfile.flush();
    }
    std::cerr << "Finished indexing the ntriple file. " << std::endl;
    std::cerr <<
        std::format("total data processed:\n"
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
