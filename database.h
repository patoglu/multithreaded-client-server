struct _database
{
    char ***table;
    char **column_names; //needs to be freed, maximum column name will be 128 like in SQL
    unsigned col;
    unsigned row;
};
