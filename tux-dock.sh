#!/bin/bash

while true; do
    echo "Docker Management Menu"
    echo "1. Pull Docker Image"
    echo "2. Run/Create Docker Container Interactively"
    echo "3. List All Docker Containers"
    echo "4. List All Docker Images"
    echo "5. Start Interactive Container Session"
    echo "6. Start Detached Container Session"
    echo "7. Delete Docker Image"
    echo "8. Stop Docker Container"
    echo "9. Remove Docker Container"
    echo "10. Access Interactive Shell of Running Container"
    echo "11. Spin Up MySQL Docker Container"
    echo "12. Exit"
    read -p "Choose an option: " option

    case $option in
        1)
            read -p "Enter the Docker image to pull (e.g., alpine): " image
            docker pull $image
            ;;
        2)
            read -p "Enter the Docker image to run interactively (e.g., alpine): " image
            read -p "How many port mappings do you want to add? " port_count
            ports=""
            for ((i=1; i<=port_count; i++)); do
                read -p "Enter port mapping #$i (e.g., 8081:8081): " port
                ports+="-p $port "
            done
            docker run -it $ports $image /bin/sh
            ;;
        3)
            docker ps -a
            ;;
        4)
            docker images
            ;;
        5)
            read -p "Enter the container ID to start interactively: " containerid
            docker start -ai $containerid
            ;;
        6)
            read -p "Enter the container ID to start in detached mode: " containerid
            docker start $containerid
            ;;
        7)
            read -p "Enter the Docker image name or ID to delete: " image
            docker rmi $image
            ;;
        8)
            read -p "Enter the container ID to stop: " containerid
            docker stop $containerid
            ;;
        9)
            read -p "Enter the container ID to remove: " containerid
            docker rm $containerid
            ;;
        10)
            read -p "Enter the container ID to access an interactive shell: " containerid
            docker exec -it $containerid /bin/sh
            ;;
        11)
            read -p "Enter the port range (e.g., 3306:3306): " port
            read -p "Enter the MySQL root password: " mysql_password
            read -p "Enter the MySQL version tag (e.g., 8): " mysql_version
            docker run -p $port --name mysql-container -e MYSQL_ROOT_PASSWORD=$mysql_password -d mysql:$mysql_version
            ;;
        12)
            break
            ;;
        *)
            echo "Invalid option. Please try again."
            ;;
    esac
done
