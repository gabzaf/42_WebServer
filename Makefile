NAME		= webserv
CPP		= c++
CPPFLAGS	= -Wall -Wextra -Werror -std=c++98 -Iincludes
RM		= rm -rf

SRCS_DIR	= srcs
OBJS_DIR	= objs
INCS_DIR	= includes
SRCS = main.cpp \
       Server.cpp \
       Config.cpp \
       HttpRequest.cpp \
       HttpResponse.cpp \
       CgiHandler.cpp
OBJS		= $(addprefix $(OBJS_DIR)/, $(SRCS:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CPP) $(CPPFLAGS) $(OBJS) -o $(NAME)
	@echo "Webserv compiled successfully!"

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.cpp | $(OBJS_DIR)
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

clean:
	$(RM) $(OBJS_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
