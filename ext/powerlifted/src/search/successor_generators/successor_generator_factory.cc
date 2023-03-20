
#include "successor_generator_factory.h"

#include "full_reducer_successor_generator.h"
#include "naive_successor.h"
#include "yannakakis.h"

#include "../database/table.h"

#include <iostream>

#include <boost/algorithm/string.hpp>

SuccessorGenerator *SuccessorGeneratorFactory::create(const std::string &method,
                                                      unsigned seed,
                                                      Task &task)
{
    std::cout << "Creating successor generator factory..." << std::endl;
    if (boost::iequals(method, "join")) {
        return new NaiveSuccessorGenerator(task);
    }
    else if (boost::iequals(method, "full_reducer")) {
        return new FullReducerSuccessorGenerator(task);
    }
    else if (boost::iequals(method, "yannakakis")) {
        return new YannakakisSuccessorGenerator(task);
    }
    else {
        std::cerr << "Invalid successor generator method \"" << method << "\"" << std::endl;
        exit(-1);
    }
}
