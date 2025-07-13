from pgmpy.readwrite import BIFReader
from pgmpy.inference import VariableElimination

# 1. Carica la rete Bayesiana da file BIF
reader = BIFReader("asia.bif")  # metti il path corretto al tuo file .bif
model = reader.get_model()

# 2. Crea l’oggetto per inferenza
infer = VariableElimination(model)

# 3. Calcola le probabilità marginali per ogni variabile
variables = model.nodes()
for var in variables:
    marginal = infer.query(variables=[var], show_progress=False)
    print(f"\nMarginal distribution for {var}:")
    print(marginal)