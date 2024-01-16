#include <array>
#include <cassert>
#include <filesystem>
#include <iostream>

#include <boost/program_options.hpp>
#include <cfitsio/fitsio.h>


#include <nanovdb/util/CreateNanoGrid.h>
#include <nanovdb/util/GridBuilder.h>
#include <nanovdb/util/IO.h>
#include <nanovdb/util/Primitives.h>

#include <NanoCutterParser.h>


auto fitsDeleter(fitsfile* file) -> void
{
	auto status = int{};
	ffclos(file, &status);
	assert(status == 0);
};

using unique_fitsfile = std::unique_ptr<fitsfile, decltype(&fitsDeleter)>;

#define logError(status)                                                                                               \
	do                                                                                                                 \
	{                                                                                                                  \
		std::array<char, 30> errorMsg;                                                                                 \
		fits_get_errstatus(status, errorMsg.data());                                                                   \
		std::cout << errorMsg.data() << std::endl;                                                                     \
	}                                                                                                                  \
	while (0)
namespace po = boost::program_options;

enum class CuttingStrategy
{
	binaryPartition,
	fitMemoryReq
};

enum class LogLevel
{
	none,
	essential,
	all
};

enum class Filter
{
	max,
	mean
};

static auto operator>>(std::istream& in, Filter& filter) -> std::istream&
{
	std::string token;
	in >> token;

	std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });

	if (token == "max")
	{
		filter = Filter::max;
	}
	else if (token == "mean")
	{
		filter = Filter::mean;
	}
	else
	{
		throw po::validation_error{ po::validation_error::kind_t::invalid_option_value };
	}

	return in;
}

static auto operator>>(std::istream& in, LogLevel& level) -> std::istream&
{
	std::string token;
	in >> token;

	std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });

	if (token == "none")
	{
		level = LogLevel::none;
	}
	else if (token == "essential")
	{
		level = LogLevel::essential;
	}
	else if (token == "all")
	{
		level = LogLevel::all;
	}
	else
	{
		throw po::validation_error{ po::validation_error::kind_t::invalid_option_value };
	}

	return in;
}

static auto operator>>(std::istream& in, CuttingStrategy& strategy) -> std::istream&
{
	std::string token;
	in >> token;

	std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });

	if (token == "binary_partition")
	{
		strategy = CuttingStrategy::binaryPartition;
	}
	else if (token == "fit_memory_req")
	{
		strategy = CuttingStrategy::fitMemoryReq;
	}
	else
	{
		throw po::validation_error{ po::validation_error::kind_t::invalid_option_value };
	}

	return in;
}

auto testFitsFile(const std::filesystem::path& file) -> bool
{
	auto canOpen = false;
	fitsfile* fitsFilePtr{ nullptr };
	auto fitsError = int{};
	ffopen(&fitsFilePtr, file.generic_string().c_str(), READONLY, &fitsError);

	canOpen = fitsError == 0;

	if (canOpen)
	{
		ffclos(fitsFilePtr, &fitsError);
		assert(fitsError == 0);
	}

	return canOpen;
}

auto main(int argc, char** argv) -> int
{

	/*
	 *	program	src		|--source_fits_file file
	 *			[-r		|	--regions				regions_files...	]
	 *			[-min	|	--threshold_filter_min	value				] //??? We do not know the stored format upfront
	 * //value will be cast to the source format value
	 *			[-max	|	--threshold_filter_max	value				] //??? same
	 *			[-c		|	--clamp_to_threshold						] //???
	 *			[-m		|	--masks					masks_files...		]
	 *			[-dst	|	--storage_directory		directory			] //same as source directory otherwise
	 *			[-l		|	--log_level				none |
	 *												essential |
	 *												all					] //all includes performance counters and stat.
	 *essential is by default
	 *			=====cutting parameters============
	 *			[-s		|	--strategy				binary_partition |
	 *												fit_memory_req		] //binary_partition by default
	 *			[-f		|	--filter				mean | max			] //mean by default
	 *			[-mm	|	--max_mem				value_in_bytes		] //max memory per cut volume. required for
	 *fit_memory_req strategy
	 *			=====refitting=====================
	 *			[-rf	|	--refit_region			aabb				] //only recompute data, that touches the aabb
	 *box inside source fits data
	 *			[-rf_src|	--refit_source_dir		directory			]
	 */


	struct Config
	{
		std::filesystem::path src{};
		std::filesystem::path dst{};
		std::vector<std::filesystem::path> regions{};
		std::vector<std::filesystem::path> masks{};
		LogLevel logLevel{ LogLevel::essential };
		double min{ 0.0 };
		double max{ 0.0 };
		bool clamp{ false };

		CuttingStrategy strategy;
		unsigned int maxMemInBytes;
		Filter filter;
	};

	auto cutterConfig = Config{};


	auto generalConfigurations = po::options_description{ "General Configurations" };
	generalConfigurations.add_options()("source_fits_file,src",
										po::value<std::filesystem::path>(&cutterConfig.src)->required());
	generalConfigurations.add_options()("storage_directory,dst", po::value<std::filesystem::path>(&cutterConfig.dst));
	generalConfigurations.add_options()(
		"regions, r", po::value<std::vector<std::filesystem::path>>(&cutterConfig.regions)->composing());
	generalConfigurations.add_options()(
		"masks, m", po::value<std::vector<std::filesystem::path>>(&cutterConfig.masks)->composing());
	generalConfigurations.add_options()("threshold_filter_min,min", po::value<double>(&cutterConfig.min));
	generalConfigurations.add_options()("threshold_filter_max,max", po::value<double>(&cutterConfig.max));
	generalConfigurations.add_options()("clamp_to_threshold,c",
										po::value<bool>(&cutterConfig.clamp)->implicit_value(true, "true"));
	generalConfigurations.add_options()(
		"log_level", po::value<LogLevel>(&cutterConfig.logLevel)->default_value(LogLevel::essential, "essential"));
	generalConfigurations.add_options()("help", "produce help message");

	auto cuttingConfigurations = po::options_description{ "Cutting Configurations" };
	cuttingConfigurations.add_options()("strategy",
										po::value<CuttingStrategy>(&cutterConfig.strategy)
											->default_value(CuttingStrategy::binaryPartition, "binary_partition"),
										"possible values are binary_partition and fit_memory_req");
	cuttingConfigurations.add_options()("filter",
										po::value<Filter>(&cutterConfig.filter)->default_value(Filter::mean, "mean"));
	cuttingConfigurations.add_options()(
		"max_mem", po::value<unsigned int>(&cutterConfig.maxMemInBytes)->default_value(0),
		"Max memory in bytes per cut volume. This option should be set for fit_memory_req strategy");

	auto cmdOptions = po::options_description{}.add(generalConfigurations).add(cuttingConfigurations);
	auto vm = po::variables_map{};
	po::store(po::command_line_parser(argc, argv).options(cmdOptions).allow_unregistered().run(), vm);
	po::notify(vm);

	std::cout << cmdOptions << std::endl;

	if (vm.count("src"))

		if (vm.count("help"))
		{
			std::cout << cmdOptions << "\n";
			return 1;
		}

	if (vm.count("source_fits_file"))
	{
		auto isValid = false;
		if (exists(cutterConfig.src) && cutterConfig.src.has_filename())
		{
			isValid = testFitsFile(cutterConfig.src);
		}
		if (!vm.count("storage_directory") && isValid)
		{
			cutterConfig.dst = cutterConfig.src.parent_path() / "cut_output";
		}

		if (!isValid)
		{
			std::cout << "Source Fits file [" << cutterConfig.src.string() << "]"
					  << "is not valid!" << std::endl;
			return EXIT_FAILURE;
		}
	}

	if (vm.count("storage_directory"))
	{
		if (!exists(cutterConfig.dst))
		{
			create_directory(cutterConfig.dst);
		}

		const auto isDstValid = is_directory(cutterConfig.dst) &&
			std::filesystem::perms::owner_write ==
				(status(cutterConfig.dst).permissions() & std::filesystem::perms::owner_write);
		if (!isDstValid)
		{
			std::cout << "Storage directory [" << cutterConfig.dst.string() << "]"
					  << "is not valid!" << std::endl;
			return EXIT_FAILURE;
		}
	}

	const auto fitsFilePath = std::filesystem::path{ "D:/datacubes/testDataSet/n4565.fits" };
	const auto catalogFilePath = std::filesystem::path{ "D:/datacubes/testDataSet/n4565_catalog.fits" };
	const auto maskFilePath = std::filesystem::path{ "D:/datacubes/testDataSet/n4565_mask.fits" };


	fitsfile* fitsFilePtr{ nullptr };
	auto fitsError = int{};
	ffopen(&fitsFilePtr, fitsFilePath.generic_string().c_str(), READONLY, &fitsError);
	assert(fitsError == 0);

	auto fitsFile = unique_fitsfile(fitsFilePtr, &fitsDeleter);

	int axisCount;
	int imgType;
	long axis[3];
	fits_get_img_param(fitsFile.get(), 3, &imgType, &axisCount, &axis[0], &fitsError);

	// const auto background = 5.0f;

	// const int size = 500;
	//       auto func = [&](const nanovdb::Coord& ijk)
	//{
	//	float v = 40.0f +
	//		50.0f *
	//			(cos(ijk[0] * 0.1f) * sin(ijk[1] * 0.1f) + cos(ijk[1] * 0.1f) * sin(ijk[2] * 0.1f) +
	//			 cos(ijk[2] * 0.1f) * sin(ijk[0] * 0.1f));
	//	v = nanovdb::Max(v, nanovdb::Vec3f(ijk).length() - size); // CSG intersection with a sphere
	//	return v > background ? background : v < -background ? -background : v; // clamp value
	//};
	// nanovdb::build::Grid<float> grid(background, "funny", nanovdb::GridClass::LevelSet);
	//       grid(func, nanovdb::CoordBBox(nanovdb::Coord(-size), nanovdb::Coord(size)));

	auto g = nanovdb::createFogVolumeSphere(10.0f, nanovdb::Vec3d(-20, 0, 0), 1.0, 3.0, nanovdb::Vec3d(0), "sphere");

	nanovdb::io::writeGrid((cutterConfig.dst / "funny.nvdb").string(), g,
						   nanovdb::io::Codec::NONE); // TODO: enable nanovdb::io::Codec::BLOSC


	cutterParser::TreeNode c1;
	c1.nanoVdbFile = "funny.nvdb";
	cutterParser::TreeNode c2;
	c2.nanoVdbFile = "funny.nvdb";

	

	cutterParser::TreeNode n;
	n.nanoVdbFile = "funny.nvdb";
	n.children.push_back(c1);
	n.children.push_back(c2);

	cutterParser::store(cutterConfig.dst/"project.b3d", n);

	int bitpix; // BYTE_IMG (8), SHORT_IMG (16), LONG_IMG (32), LONGLONG_IMG (64), FLOAT_IMG (-32)

	{
		auto status = 0;
		if (fits_get_img_equivtype(fitsFile.get(), &bitpix, &status))
		{
			logError(status);
		}
	}

	{
		auto nan = NAN;
		auto status = 0;
		/*if (fits_read_subset(fitsFile, TFLOAT, firstPx, lastPx, inc, &nan, dataBuffer.data() + nextDataIdx, 0,
		&status))
		{
			logError(status);
		}*/
	}
	// fits_read_subset(fitsFile.get(), type, )

	// fits_get_img_param()
	return EXIT_SUCCESS;
}