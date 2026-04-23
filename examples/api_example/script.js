const fetchBtn = document.getElementById('fetchBtn');
const nameInput = document.getElementById('userName');
const messageDisplay = document.getElementById('messageDisplay');
const resultBox = document.getElementById('resultBox');
const statusIndicator = document.getElementById('statusIndicator');
const dot = document.getElementById('dot');
const statusText = document.getElementById('statusText');

fetchBtn.addEventListener('click', () => {
    const nameValue = nameInput.value.trim();

    // Simple validation
    if (!nameValue) {
        nameInput.classList.add('border-red-500', 'animate-shake');
        setTimeout(() => nameInput.classList.remove('animate-shake'), 500);
        return;
    } else {
        nameInput.classList.remove('border-red-500');
    }

    // UI Loading
    statusIndicator.classList.remove('hidden');
    dot.className = "h-2 w-2 rounded-full animate-pulse bg-blue-500";
    statusText.textContent = "Calling...";
    statusText.className = "text-blue-600";
    messageDisplay.textContent = "The server is preparing the response...";

    fetch('http://localhost:8081/api/greet', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ name: nameValue }), // Using the input variable
    })
    .then(response => {
        if (!response.ok) throw new Error('Server error');
        return response.json();
    })
    .then(result => {
        // UI Success
        dot.className = "h-2 w-2 rounded-full bg-emerald-500";
        statusText.textContent = "Connected";
        statusText.className = "text-emerald-600 font-bold";
        
        resultBox.className = "min-h-[100px] bg-emerald-50 border-2 border-emerald-200 rounded-2xl flex items-center justify-center p-6 text-center shadow-inner";
        messageDisplay.className = "text-emerald-900 font-semibold text-lg leading-tight";
        messageDisplay.textContent = result.message;
    })
    .catch(error => {
        // UI Error
        dot.className = "h-2 w-2 rounded-full bg-red-500";
        statusText.textContent = "Failed";
        statusText.className = "text-red-600 font-bold";
        
        resultBox.className = "min-h-[100px] bg-red-50 border-2 border-red-200 rounded-2xl flex items-center justify-center p-6 text-center";
        messageDisplay.className = "text-red-700 text-sm font-medium";
        messageDisplay.textContent = "Error connecting to port 8081. Is the API running?";
    });
});

// Allow confirmation with the "Enter" key
nameInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') fetchBtn.click();
});