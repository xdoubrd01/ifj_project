Type get_variable_type(char* var_name) {
    if (strcmp(var_name, "a") == 0) return TYPE_INT;
    return TYPE_UNDEFINED; 
}