import socket
import tkinter as tk
from tkinter import ttk, messagebox


class ClienteEstufaApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Cliente - Estufa Inteligente")
        self.root.geometry("760x620")
        self.root.resizable(False, False)

        self.socket_cliente = None
        self.conectado = False

        self.criar_interface()

    def criar_interface(self):
        frame_conexao = ttk.LabelFrame(self.root, text="Conexão com o servidor")
        frame_conexao.pack(fill="x", padx=10, pady=10)

        ttk.Label(frame_conexao, text="IP:").grid(row=0, column=0, padx=5, pady=8, sticky="w")
        self.entrada_ip = ttk.Entry(frame_conexao, width=18)
        self.entrada_ip.insert(0, "127.0.0.1")
        self.entrada_ip.grid(row=0, column=1, padx=5, pady=8)

        ttk.Label(frame_conexao, text="Porta:").grid(row=0, column=2, padx=5, pady=8, sticky="w")
        self.entrada_porta = ttk.Entry(frame_conexao, width=10)
        self.entrada_porta.insert(0, "5000")
        self.entrada_porta.grid(row=0, column=3, padx=5, pady=8)

        self.botao_conectar = ttk.Button(
            frame_conexao,
            text="Conectar",
            command=self.conectar
        )
        self.botao_conectar.grid(row=0, column=4, padx=5, pady=8)

        self.botao_desconectar = ttk.Button(
            frame_conexao,
            text="Desconectar",
            command=self.desconectar,
            state="disabled"
        )
        self.botao_desconectar.grid(row=0, column=5, padx=5, pady=8)

        self.label_status_conexao = ttk.Label(
            frame_conexao,
            text="Status: desconectado"
        )
        self.label_status_conexao.grid(row=1, column=0, columnspan=6, padx=5, pady=5, sticky="w")

        frame_estado = ttk.LabelFrame(self.root, text="Estado atual da estufa")
        frame_estado.pack(fill="x", padx=10, pady=5)

        self.var_temperatura = tk.StringVar(value="Temperatura: --")
        self.var_umidade = tk.StringVar(value="Umidade: --")
        self.var_luminosidade = tk.StringVar(value="Luminosidade: --")
        self.var_aquecedor = tk.StringVar(value="Aquecedor: --")
        self.var_ventilador = tk.StringVar(value="Ventilador: --")
        self.var_bomba = tk.StringVar(value="Bomba: --")
        self.var_modo = tk.StringVar(value="Modo: --")
        self.var_temp_desejada = tk.StringVar(value="Temperatura desejada: --")
        self.var_alarme = tk.StringVar(value="Alarme: --")

        labels_estado = [
            self.var_temperatura,
            self.var_umidade,
            self.var_luminosidade,
            self.var_aquecedor,
            self.var_ventilador,
            self.var_bomba,
            self.var_modo,
            self.var_temp_desejada,
            self.var_alarme
        ]

        for i, variavel in enumerate(labels_estado):
            linha = i // 3
            coluna = i % 3

            ttk.Label(
                frame_estado,
                textvariable=variavel,
                width=30
            ).grid(row=linha, column=coluna, padx=8, pady=6, sticky="w")

        frame_comandos = ttk.LabelFrame(self.root, text="Comandos")
        frame_comandos.pack(fill="x", padx=10, pady=5)

        ttk.Button(
            frame_comandos,
            text="Atualizar STATUS",
            command=lambda: self.enviar_comando("STATUS")
        ).grid(row=0, column=0, padx=5, pady=5, sticky="ew")

        ttk.Button(
            frame_comandos,
            text="Modo Automático",
            command=lambda: self.enviar_comando("AUTO_ON")
        ).grid(row=0, column=1, padx=5, pady=5, sticky="ew")

        ttk.Button(
            frame_comandos,
            text="Modo Manual",
            command=lambda: self.enviar_comando("AUTO_OFF")
        ).grid(row=0, column=2, padx=5, pady=5, sticky="ew")

        ttk.Button(
            frame_comandos,
            text="Ligar Aquecedor",
            command=lambda: self.enviar_comando("HEATER_ON")
        ).grid(row=1, column=0, padx=5, pady=5, sticky="ew")

        ttk.Button(
            frame_comandos,
            text="Desligar Aquecedor",
            command=lambda: self.enviar_comando("HEATER_OFF")
        ).grid(row=1, column=1, padx=5, pady=5, sticky="ew")

        ttk.Button(
            frame_comandos,
            text="Ligar Ventilador",
            command=lambda: self.enviar_comando("FAN_ON")
        ).grid(row=2, column=0, padx=5, pady=5, sticky="ew")

        ttk.Button(
            frame_comandos,
            text="Desligar Ventilador",
            command=lambda: self.enviar_comando("FAN_OFF")
        ).grid(row=2, column=1, padx=5, pady=5, sticky="ew")

        ttk.Button(
            frame_comandos,
            text="Ligar Bomba",
            command=lambda: self.enviar_comando("PUMP_ON")
        ).grid(row=3, column=0, padx=5, pady=5, sticky="ew")

        ttk.Button(
            frame_comandos,
            text="Desligar Bomba",
            command=lambda: self.enviar_comando("PUMP_OFF")
        ).grid(row=3, column=1, padx=5, pady=5, sticky="ew")

        ttk.Button(
            frame_comandos,
            text="Resetar Alarme",
            command=lambda: self.enviar_comando("RESET_ALARM")
        ).grid(row=3, column=2, padx=5, pady=5, sticky="ew")

        ttk.Label(frame_comandos, text="Temperatura desejada:").grid(
            row=4, column=0, padx=5, pady=8, sticky="w"
        )

        self.entrada_temp = ttk.Entry(frame_comandos, width=10)
        self.entrada_temp.insert(0, "30")
        self.entrada_temp.grid(row=4, column=1, padx=5, pady=8, sticky="w")

        ttk.Button(
            frame_comandos,
            text="Enviar SET_TEMP",
            command=self.enviar_temperatura
        ).grid(row=4, column=2, padx=5, pady=8, sticky="ew")

        for coluna in range(3):
            frame_comandos.columnconfigure(coluna, weight=1)

        frame_log = ttk.LabelFrame(self.root, text="Respostas do servidor")
        frame_log.pack(fill="both", expand=True, padx=10, pady=10)

        self.texto_log = tk.Text(frame_log, height=12, wrap="word")
        self.texto_log.pack(side="left", fill="both", expand=True, padx=5, pady=5)

        barra_rolagem = ttk.Scrollbar(frame_log, command=self.texto_log.yview)
        barra_rolagem.pack(side="right", fill="y")
        self.texto_log.configure(yscrollcommand=barra_rolagem.set)

        self.root.protocol("WM_DELETE_WINDOW", self.fechar_janela)

    def registrar_log(self, mensagem):
        self.texto_log.insert("end", mensagem + "\n")
        self.texto_log.see("end")

    def conectar(self):
        if self.conectado:
            return

        ip = self.entrada_ip.get().strip()
        porta_texto = self.entrada_porta.get().strip()

        if not ip:
            messagebox.showerror("Erro", "Informe o IP do servidor.")
            return

        try:
            porta = int(porta_texto)
        except ValueError:
            messagebox.showerror("Erro", "A porta deve ser um número.")
            return

        try:
            self.socket_cliente = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket_cliente.settimeout(3)
            self.socket_cliente.connect((ip, porta))

            self.conectado = True
            self.label_status_conexao.config(text=f"Status: conectado em {ip}:{porta}")
            self.botao_conectar.config(state="disabled")
            self.botao_desconectar.config(state="normal")

            resposta_inicial = self.receber_resposta()
            if resposta_inicial:
                self.registrar_log("[SERVIDOR] " + resposta_inicial)

            self.enviar_comando("STATUS")

        except Exception as erro:
            self.conectado = False
            self.socket_cliente = None
            messagebox.showerror("Erro de conexão", f"Não foi possível conectar ao servidor.\n\n{erro}")

    def desconectar(self):
        if self.socket_cliente:
            try:
                self.enviar_comando("QUIT")
            except Exception:
                pass

            try:
                self.socket_cliente.close()
            except Exception:
                pass

        self.socket_cliente = None
        self.conectado = False

        self.label_status_conexao.config(text="Status: desconectado")
        self.botao_conectar.config(state="normal")
        self.botao_desconectar.config(state="disabled")

        self.registrar_log("[CLIENTE] Desconectado do servidor.")

    def receber_resposta(self):
        if not self.socket_cliente:
            return ""

        dados = b""

        while b"\n" not in dados:
            parte = self.socket_cliente.recv(1024)

            if not parte:
                break

            dados += parte

        return dados.decode("utf-8", errors="replace").strip()

    def enviar_comando(self, comando):
        if not self.conectado or not self.socket_cliente:
            messagebox.showwarning("Aviso", "Conecte ao servidor antes de enviar comandos.")
            return

        try:
            mensagem = comando + "\n"
            self.socket_cliente.sendall(mensagem.encode("utf-8"))

            self.registrar_log("[CLIENTE] " + comando)

            resposta = self.receber_resposta()

            if resposta:
                self.registrar_log("[SERVIDOR] " + resposta)

                if resposta.startswith("STATUS:"):
                    self.atualizar_estado(resposta)

            if comando == "QUIT":
                self.conectado = False

        except Exception as erro:
            self.registrar_log(f"[ERRO] Falha ao enviar comando: {erro}")
            messagebox.showerror("Erro", f"Falha na comunicação com o servidor.\n\n{erro}")
            self.desconectar_forcado()

    def enviar_temperatura(self):
        valor = self.entrada_temp.get().strip()

        if not valor:
            messagebox.showwarning("Aviso", "Informe a temperatura desejada.")
            return

        try:
            float(valor.replace(",", "."))
        except ValueError:
            messagebox.showerror("Erro", "Digite um valor numérico para a temperatura.")
            return

        valor = valor.replace(",", ".")
        self.enviar_comando(f"SET_TEMP {valor}")

    def atualizar_estado(self, resposta):
        conteudo = resposta.replace("STATUS:", "").strip()
        partes = conteudo.split(";")

        dados = {}

        for parte in partes:
            parte = parte.strip()

            if "=" in parte:
                chave, valor = parte.split("=", 1)
                dados[chave.strip()] = valor.strip()

        self.var_temperatura.set("Temperatura: " + dados.get("TEMP", "--"))
        self.var_umidade.set("Umidade: " + dados.get("UMIDADE", "--"))
        self.var_luminosidade.set("Luminosidade: " + dados.get("LUMINOSIDADE", "--"))
        self.var_aquecedor.set("Aquecedor: " + dados.get("AQUECEDOR", "--"))
        self.var_ventilador.set("Ventilador: " + dados.get("VENTILADOR", "--"))
        self.var_bomba.set("Bomba: " + dados.get("BOMBA", "--"))
        self.var_modo.set("Modo: " + dados.get("MODO", "--"))
        self.var_temp_desejada.set("Temperatura desejada: " + dados.get("TEMP_DESEJADA", "--"))
        self.var_alarme.set("Alarme: " + dados.get("ALARME", "--"))

    def desconectar_forcado(self):
        try:
            if self.socket_cliente:
                self.socket_cliente.close()
        except Exception:
            pass

        self.socket_cliente = None
        self.conectado = False

        self.label_status_conexao.config(text="Status: desconectado")
        self.botao_conectar.config(state="normal")
        self.botao_desconectar.config(state="disabled")

    def fechar_janela(self):
        if self.conectado:
            try:
                self.desconectar()
            except Exception:
                pass

        self.root.destroy()


if __name__ == "__main__":
    janela = tk.Tk()
    app = ClienteEstufaApp(janela)
    janela.mainloop()
