% % Projeto do compensador

s = tf('s');
Gcinv = 235/(s * 2.2e-3);              %% Ganho de corrente
Ganta = (2*pi*15e3)/(s + 2*pi*15e3);     %% Ganho filtro anti aliasing

Gtot = Ganta * Gcinv;                    %% Ganho total
bodeplot(Gtot)

% passo 2: frequencia de cruzamento desejada
fc = 2e3;

% passo 3: Avanco
avanco = -90 + 30 -(-97.6);               %% 97.6° é a fase em 2k do inversor
                                          %%30° é a margem de fase desejada 

% passo 4: determinacao ganho compensador e pwm
Kpwm = 1 / 1000;                         %% Ganho do PWM
Kadc = 4096/3.3;                         %% Ganho do ADC
Klem = 1.06;                             %% Ganho do sensor de corrente
Kcond = 0.870;
Hi = tf(Kpwm * Kadc * Klem * Kcond)     %% Ganho da realimentação (H)

Gp = 8.414                      %% Ganho da planta em 2kHz
Gc = 1/(Gp * Hi)                        %% Ganho do compensador em 2kHz

% passo 5: calculo Kpz
Kpz = tan(((avanco*pi/180)/2) + (pi/4)) %% Distancia entre o polo e zero, circuito tipo 2

% passo 6: Reposicionamento polos e zeros
fz = fc / Kpz                           %% Frequencia do zero
fp = fc * Kpz                          %% Frequencia do polo
Kc = Gc*2*pi*fz                         %% Ganho do compensador

fzc = (1/(pi/30e3))*tan(fz*pi/30e3)   %% Frequencia do zero compensada pra discretização
fpc = (1/(pi/30e3))*tan(fp*pi/30e3)    %% Frequencia do polo compensada pra discretização

Cc = (Kc/s) * ((1 + (s/(2*pi*fzc)))/(1 + (s/(2*pi*fpc)))) %% Função de transferência do compensador em s
bode(Cc*Gtot)

% passo 7: Discretização

GCz = c2d(Cc,1/30e3,'tustin')

