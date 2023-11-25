from locust import HttpUser, task, between

class MyUser(HttpUser):
    wait_time = between(1, 3)
    host = "http://localhost:8080"  # Adicione o host base aqui

    @task
    def get_image(self):
        self.client.get("/image.jpg")
